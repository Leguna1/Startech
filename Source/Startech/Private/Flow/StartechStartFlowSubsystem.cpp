#include "Flow/StartechStartFlowSubsystem.h"

#include "Bounties/BountyBoardSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Save/QuestsSaveParticipant.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Flow/StartechInputState.h"
#include "Flow/StartechMenuGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/SaveSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveSubsystem.h"
#include "StartechQuestBridge.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Station.h"
#include "TimerManager.h"

namespace StartFlow
{
#if !UE_BUILD_SHIPPING
	/** Test aid: console commands (separated by ';') run once the game has started, e.g. "Startech.Save.Dump;quit". */
	static TAutoConsoleVariable<FString> CVarOnStartCommands(TEXT("Startech.Flow.OnStartCommands"), TEXT(""),
		TEXT("Test aid: console commands, separated by ';', run when the game map has started (player docked). "
			"For example: Startech.Flow.InputState;Startech.Save.Dump;quit"));

	/** Test aid: a game map started without a slot loads this slot (as Continue would) instead of the test game. */
	static TAutoConsoleVariable<int32> CVarLoadSlotOnStart(TEXT("Startech.Flow.LoadSlotOnStart"), -1,
		TEXT("Test aid: a game map started with no active slot loads this slot instead of starting the [PIE] test game (-1: off). "
			"Set it before the map starts, e.g. -ini:Engine:[ConsoleVariables]:Startech.Flow.LoadSlotOnStart=3"));
#endif

	/** Seconds between RequestSector attempts while the director is busy. */
	constexpr float RetrySeconds = 0.1f;

	/** Seconds the screen takes to fade in once the player is docked. */
	constexpr float FadeInSeconds = 1.f;
}

bool UStartechStartFlowSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UStartechStartFlowSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (Cast<AStartechMenuGameMode>(InWorld.GetAuthGameMode()))
	{
		return; // The main menu.
	}
	bActive = true;

	if (USectorDirectorSubsystem* const Director = GetDirector())
	{
		Director->OnSectorReady.AddUniqueDynamic(this, &UStartechStartFlowSubsystem::HandleSectorReady);
	}

	USaveSubsystem* const Saves = GetSaves();
	if (!Saves)
	{
		UE_LOG(LogStartechSave, Error, TEXT("Start flow: no save subsystem; requesting the home sector straight away."));
		RequestHomeSector();
		return;
	}

	// Before any actor's BeginPlay, so the ship's participants (the last to register) trigger this.
	Saves->OnAllParticipantsLoaded.AddUniqueDynamic(this, &UStartechStartFlowSubsystem::HandleAllParticipantsLoaded);

#if !UE_BUILD_SHIPPING
	if (const int32 TestLoadSlot = StartFlow::CVarLoadSlotOnStart.GetValueOnGameThread(); !Saves->HasActiveSlot() && TestLoadSlot >= 0)
	{
		FText Reason;
		if (!Saves->LoadSlot(TestLoadSlot, Reason))
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Startech.Flow.LoadSlotOnStart %d: %s"), TestLoadSlot, *Reason.ToString());
		}
	}
#endif

	if (!Saves->HasActiveSlot())
	{
		if (UStartechSaveSubsystem* const StartechSaves = UStartechSaveSubsystem::Get(&InWorld))
		{
			StartechSaves->StartTestGame();
		}
	}
	UE_LOG(LogStartechSave, Log, TEXT("Start flow: waiting for the save participants (slot %d)."), Saves->GetActiveSlot());
}

void UStartechStartFlowSubsystem::Deinitialize()
{
	if (USaveSubsystem* const Saves = GetSaves())
	{
		Saves->OnAllParticipantsLoaded.RemoveDynamic(this, &UStartechStartFlowSubsystem::HandleAllParticipantsLoaded);
	}
	if (USectorDirectorSubsystem* const Director = GetDirector())
	{
		Director->OnSectorReady.RemoveDynamic(this, &UStartechStartFlowSubsystem::HandleSectorReady);
	}
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	StartCommands.Stop();
	Super::Deinitialize();
}

void UStartechStartFlowSubsystem::HandleAllParticipantsLoaded()
{
	if (!bActive || bSectorRequested)
	{
		return;
	}
	FadeTo(1.f, 0.f);
	SetShipInputLocked(true);

	// Docking during the start (the station can appear around the ship while the sector generates, then the ship is
	// placed at it) is not a visit: the saved bounty offers must survive it. Lifted once the ship is placed.
	if (UStartechQuestBridge* const QuestBridge = UStartechQuestBridge::Get(this))
	{
		QuestBridge->SetDockRefreshSuppressed(true);
	}
	RequestHomeSector();
}

void UStartechStartFlowSubsystem::RequestHomeSector()
{
	USectorDirectorSubsystem* const Director = GetDirector();
	const FName Home = UStartechSettings::Get().HomeSectorId;
	if (!Director)
	{
		UE_LOG(LogStartechSave, Warning, TEXT("Start flow: no sector director in this level; the ship stays where it is."));
		bStarted = true;
		FadeTo(0.f, StartFlow::FadeInSeconds);
		ReleasePlayer();
		OnGameStarted.Broadcast();
		return;
	}

	if (!Director->RequestSector(Home))
	{
		// Busy (or an unknown id, which the director logs): try again shortly.
		GetWorld()->GetTimerManager().SetTimer(RetryTimer, this, &UStartechStartFlowSubsystem::RequestHomeSector,
			StartFlow::RetrySeconds, false);
		return;
	}
	bSectorRequested = true;
	UE_LOG(LogStartechSave, Log, TEXT("Start flow: every participant is loaded; generating '%s'."), *Home.ToString());
}

void UStartechStartFlowSubsystem::HandleSectorReady(const FName SectorId)
{
	if (!bSectorRequested || bStarted || SectorId != UStartechSettings::Get().HomeSectorId)
	{
		return;
	}
	bStarted = true;

	if (AStartechShipPawn* const Ship = GetShip())
	{
		PlaceShipAtStation(*Ship);
	}
	if (UStartechQuestBridge* const QuestBridge = UStartechQuestBridge::Get(this))
	{
		QuestBridge->SetDockRefreshSuppressed(false);
	}
	ReleasePlayer();

	// A new game (or an old save whose bounties were dropped) has no saved offers to keep: fill the board as a first
	// visit would.
	const UStartechSaveSubsystem* const StartechSaves = UStartechSaveSubsystem::Get(this);
	const UQuestsSaveParticipant* const QuestsSave = StartechSaves ? StartechSaves->GetQuestsParticipant() : nullptr;
	UBountyBoardSubsystem* const Board = GetWorld()->GetGameInstance()->GetSubsystem<UBountyBoardSubsystem>();
	if (QuestsSave && QuestsSave->NeedsBoardRefresh() && Board)
	{
		Board->Refresh();
	}
	FadeTo(0.f, StartFlow::FadeInSeconds);

	UE_LOG(LogStartechSave, Log, TEXT("Start flow: '%s' is ready; the player is %s."), *SectorId.ToString(),
		GetShip() && GetShip()->IsDocked() ? TEXT("docked at the station") : TEXT("NOT docked (no station found)"));
	UE_LOG(LogStartechSave, Log, TEXT("Start flow: input on arrival: %s"), *FStartechInputState::Describe(GetWorld()));
	OnGameStarted.Broadcast();

#if !UE_BUILD_SHIPPING
	StartCommands.Start(StartFlow::CVarOnStartCommands.GetValueOnGameThread(), [WeakThis = TWeakObjectPtr<UStartechStartFlowSubsystem>(this)]()
	{
		return WeakThis.IsValid() ? WeakThis->GetWorld() : nullptr;
	});
#endif
}

void UStartechStartFlowSubsystem::PlaceShipAtStation(AStartechShipPawn& Ship) const
{
	const AStation* const Station = AStation::FindNearestInPlay(*GetWorld(), Ship.GetActorLocation());
	if (!Station)
	{
		UE_LOG(LogStartechSave, Warning, TEXT("Start flow: no station in the home sector; the ship stays where it is."));
		return;
	}

	// The bounty refresh on docking is suppressed for the whole start (HandleAllParticipantsLoaded).
	Ship.PlaceAt(Station->GetRespawnTransform());
}

void UStartechStartFlowSubsystem::ReleasePlayer() const
{
	APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0);
	AStartechShipPawn* const Ship = GetShip();
	if (!PC)
	{
		return;
	}

	// The ship is the default pawn and is possessed on login; repair it if anything ever changed that.
	AStartechShipPawn* const Placed = Ship ? Ship : Cast<AStartechShipPawn>(UGameplayStatics::GetActorOfClass(this, AStartechShipPawn::StaticClass()));
	if (Placed && PC->GetPawn() != Placed)
	{
		UE_LOG(LogStartechSave, Warning, TEXT("Start flow: the controller did not possess %s; possessing it."), *Placed->GetName());
		PC->Possess(Placed);
	}

	SetShipInputLocked(false);

	// Whatever screen came before (the main menu's UI-only mode makes the game viewport ignore input, and the viewport
	// outlives map travel), the player now flies: game-only input, no cursor, no stale keys, focus on the viewport.
	FStartechInputState::SetGameOnly(*PC);
}

void UStartechStartFlowSubsystem::FadeTo(const float ToAlpha, const float Seconds) const
{
	const APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0);
	APlayerCameraManager* const Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	if (Seconds <= 0.f)
	{
		// A zero-length StartCameraFade never updates FadeAmount; this sets it at once.
		Camera->SetManualCameraFade(ToAlpha, FLinearColor::Black, false);
		return;
	}
	Camera->StartCameraFade(Camera->FadeAmount, ToAlpha, Seconds, FLinearColor::Black, false, true);
}

void UStartechStartFlowSubsystem::SetShipInputLocked(const bool bLocked) const
{
	AStartechShipPawn* const Ship = GetShip();
	APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!Ship || !PC)
	{
		return;
	}
	if (bLocked)
	{
		Ship->DisableInput(PC);
	}
	else
	{
		Ship->EnableInput(PC);
	}
}

AStartechShipPawn* UStartechStartFlowSubsystem::GetShip() const
{
	return Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
}

USaveSubsystem* UStartechStartFlowSubsystem::GetSaves() const
{
	const UWorld* const World = GetWorld();
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

USectorDirectorSubsystem* UStartechStartFlowSubsystem::GetDirector() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
}
