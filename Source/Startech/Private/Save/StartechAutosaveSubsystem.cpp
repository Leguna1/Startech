#include "Save/StartechAutosaveSubsystem.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/StartechStartFlowSubsystem.h"
#include "Gates/WarpTransitionComponent.h"
#include "HealthComponent.h"
#include "HomeRespawnSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/QuestSubsystem.h"
#include "Runtime/SaveSubsystem.h"
#include "Save/QuestsSaveParticipant.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveSubsystem.h"
#include "ShopTrading.h"
#include "StartechShipPawn.h"

namespace StartechAutosave
{
	const FName Docked(TEXT("Docked"));
	const FName Undocked(TEXT("Undocked"));
	const FName QuestTurnedIn(TEXT("QuestTurnedIn"));
	const FName SectorReady(TEXT("SectorReady"));
	const FName Trade(TEXT("Trade"));
}

bool UStartechAutosaveSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UStartechAutosaveSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UStartechStartFlowSubsystem* const StartFlow = InWorld.GetSubsystem<UStartechStartFlowSubsystem>())
	{
		StartFlow->OnGameStarted.AddUniqueDynamic(this, &UStartechAutosaveSubsystem::HandleGameStarted);
	}
	TradeHandle = FShopTrading::OnTradeCompleted.AddUObject(this, &UStartechAutosaveSubsystem::HandleTradeCompleted);
}

void UStartechAutosaveSubsystem::HandleTradeCompleted(const FTradeOutcome& Outcome, AActor& Trader)
{
	// One request per trade, however many units; the save debounce merges quick trades into one save.
	if (Trader.GetWorld() == GetWorld())
	{
		RequestAutosave(StartechAutosave::Trade);
	}
}

void UStartechAutosaveSubsystem::Deinitialize()
{
	FShopTrading::OnTradeCompleted.Remove(TradeHandle);
	TradeHandle.Reset();

	if (const UWorld* const World = GetWorld())
	{
		if (UStartechStartFlowSubsystem* const StartFlow = World->GetSubsystem<UStartechStartFlowSubsystem>())
		{
			StartFlow->OnGameStarted.RemoveDynamic(this, &UStartechAutosaveSubsystem::HandleGameStarted);
		}
	}
	if (AStartechShipPawn* const Player = Ship.Get())
	{
		Player->OnDockedChanged.RemoveDynamic(this, &UStartechAutosaveSubsystem::HandleDockedChanged);
	}
	if (UWarpTransitionComponent* const WarpComponent = Warp.Get())
	{
		WarpComponent->OnWarpFinished.RemoveDynamic(this, &UStartechAutosaveSubsystem::HandleWarpFinished);
	}
	if (USectorDirectorSubsystem* const SectorDirector = Director.Get())
	{
		SectorDirector->OnSectorReady.RemoveDynamic(this, &UStartechAutosaveSubsystem::HandleSectorReady);
	}
	if (UHomeRespawnSubsystem* const HomeRespawn = Respawn.Get())
	{
		HomeRespawn->OnRespawnSequenceChanged.RemoveDynamic(this, &UStartechAutosaveSubsystem::HandleRespawnSequenceChanged);
	}
	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->OnQuestStatusChanged.RemoveDynamic(this, &UStartechAutosaveSubsystem::HandleQuestStatusChanged);
	}
	bActive = false;
	Super::Deinitialize();
}

void UStartechAutosaveSubsystem::HandleGameStarted()
{
	UWorld* const World = GetWorld();
	Ship = Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	Director = World->GetSubsystem<USectorDirectorSubsystem>();
	Respawn = World->GetSubsystem<UHomeRespawnSubsystem>();

	if (AStartechShipPawn* const Player = Ship.Get())
	{
		Player->OnDockedChanged.AddUniqueDynamic(this, &UStartechAutosaveSubsystem::HandleDockedChanged);
		Warp = Player->FindComponentByClass<UWarpTransitionComponent>();
	}
	if (UWarpTransitionComponent* const WarpComponent = Warp.Get())
	{
		WarpComponent->OnWarpFinished.AddUniqueDynamic(this, &UStartechAutosaveSubsystem::HandleWarpFinished);
	}
	if (USectorDirectorSubsystem* const SectorDirector = Director.Get())
	{
		SectorDirector->OnSectorReady.AddUniqueDynamic(this, &UStartechAutosaveSubsystem::HandleSectorReady);
	}
	if (UHomeRespawnSubsystem* const HomeRespawn = Respawn.Get())
	{
		HomeRespawn->OnRespawnSequenceChanged.AddUniqueDynamic(this, &UStartechAutosaveSubsystem::HandleRespawnSequenceChanged);
	}
	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->OnQuestStatusChanged.AddUniqueDynamic(this, &UStartechAutosaveSubsystem::HandleQuestStatusChanged);
	}
	bActive = true;

	// The arrival is a sector becoming ready too; it also writes a new game's first save at once.
	RequestAutosave(StartechAutosave::SectorReady);
}

bool UStartechAutosaveSubsystem::IsMidSequence() const
{
	const UHomeRespawnSubsystem* const HomeRespawn = Respawn.Get();
	const UWarpTransitionComponent* const WarpComponent = Warp.Get();
	const USectorDirectorSubsystem* const SectorDirector = Director.Get();
	const UStartechStartFlowSubsystem* const StartFlow = GetWorld() ? GetWorld()->GetSubsystem<UStartechStartFlowSubsystem>() : nullptr;
	const AStartechShipPawn* const Player = Ship.Get();
	return (HomeRespawn && HomeRespawn->IsRespawning())
		|| (WarpComponent && WarpComponent->IsWarping())
		|| (SectorDirector && SectorDirector->IsBusy())
		|| (StartFlow && StartFlow->IsStarting())
		|| (Player && Player->GetHealthComponent() && Player->GetHealthComponent()->IsDead());
}

void UStartechAutosaveSubsystem::RequestAutosave(const FName Reason)
{
	if (!bActive)
	{
		return;
	}
	if (IsMidSequence())
	{
		UE_LOG(LogStartechSave, Log, TEXT("Autosave '%s' held: a death, respawn, warp or generation is running."), *Reason.ToString());
		HeldReason = Reason;
		return;
	}

	// A held request keeps its reason: the event that ends a sequence (the respawn's dock, say) merges into it.
	const FName Effective = HasHeldRequest() ? HeldReason : Reason;
	USaveSubsystem* const Saves = USaveSubsystem::Get(this);
	if (Saves && Saves->HasActiveSlot())
	{
		if (Effective != Reason)
		{
			UE_LOG(LogStartechSave, Log, TEXT("Autosave requested: %s (held; released by %s)."), *Effective.ToString(), *Reason.ToString());
		}
		else
		{
			UE_LOG(LogStartechSave, Log, TEXT("Autosave requested: %s."), *Reason.ToString());
		}
		Saves->RequestSave(Effective);
	}
	HeldReason = NAME_None;
}

void UStartechAutosaveSubsystem::ReleaseHeldRequest()
{
	if (HasHeldRequest() && !IsMidSequence())
	{
		RequestAutosave(HeldReason);
	}
}

void UStartechAutosaveSubsystem::HandleDockedChanged(const bool bDocked, const FText StationName)
{
	RequestAutosave(bDocked ? StartechAutosave::Docked : StartechAutosave::Undocked);
}

void UStartechAutosaveSubsystem::HandleQuestStatusChanged(const FName QuestId, const EQuestStatus NewStatus)
{
	if (NewStatus != EQuestStatus::Completed)
	{
		return;
	}
	// RestoreStates re-announces every quest's status; those are not turn-ins.
	const UStartechSaveSubsystem* const StartechSaves = UStartechSaveSubsystem::Get(this);
	const UQuestsSaveParticipant* const QuestsSave = StartechSaves ? StartechSaves->GetQuestsParticipant() : nullptr;
	if (QuestsSave && QuestsSave->IsRestoring())
	{
		return;
	}
	RequestAutosave(StartechAutosave::QuestTurnedIn);
}

void UStartechAutosaveSubsystem::HandleSectorReady(const FName SectorId)
{
	// During a warp or a respawn this is held, and made when that ends (HandleWarpFinished, HandleRespawnSequenceChanged).
	RequestAutosave(StartechAutosave::SectorReady);
}

void UStartechAutosaveSubsystem::HandleWarpFinished(const FName SectorId)
{
	ReleaseHeldRequest();
}

void UStartechAutosaveSubsystem::HandleRespawnSequenceChanged(const bool bSequenceActive)
{
	if (!bSequenceActive)
	{
		ReleaseHeldRequest();
	}
}

UQuestSubsystem* UStartechAutosaveSubsystem::GetQuests() const
{
	const UWorld* const World = GetWorld();
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr;
}
