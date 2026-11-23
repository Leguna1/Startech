#include "Flow/StartechGameFlowSubsystem.h"

#include "Audio/StartechMusicSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/CallsignRules.h"
#include "Flow/StartechInputState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PlayerProfileSubsystem.h"
#include "Runtime/SaveSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "StartechSettings.h"

#define LOCTEXT_NAMESPACE "StartechGameFlow"

UStartechGameFlowSubsystem* UStartechGameFlowSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UStartechGameFlowSubsystem>() : nullptr;
}

void UStartechGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<USaveSubsystem>();
	Collection.InitializeDependency<UPlayerProfileSubsystem>();
	Super::Initialize(Collection);
}

TArray<FSaveSlotInfo> UStartechGameFlowSubsystem::GetVisibleSlotInfos() const
{
	USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	if (!Saves)
	{
		return TArray<FSaveSlotInfo>();
	}
	Saves->RefreshSlotInfos();

	TArray<FSaveSlotInfo> Visible = Saves->GetSlotInfos();
	Visible.SetNum(FMath::Min(Visible.Num(), UStartechSettings::Get().VisibleSaveSlots));
	return Visible;
}

int32 UStartechGameFlowSubsystem::FindMostRecentSlot() const
{
	int32 Best = INDEX_NONE;
	FDateTime BestTime;
	for (const FSaveSlotInfo& Info : GetVisibleSlotInfos())
	{
		if (Info.bOccupied && (Best == INDEX_NONE || Info.Header.SavedUtc > BestTime))
		{
			Best = Info.Header.SlotIndex;
			BestTime = Info.Header.SavedUtc;
		}
	}
	return Best;
}

bool UStartechGameFlowSubsystem::HasAnySave() const
{
	return FindMostRecentSlot() != INDEX_NONE;
}

bool UStartechGameFlowSubsystem::StartNewGame(const int32 SlotIndex, const FString& Callsign, FText& OutReason)
{
	const FString Name = FCallsignRules::Normalize(Callsign);
	USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	UPlayerProfileSubsystem* const Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
	if (!IsVisibleSlot(SlotIndex) || !Saves || !Profile)
	{
		OutReason = LOCTEXT("NoSlot", "That slot can't be used.");
		return false;
	}
	if (!FCallsignRules::IsValid(Name))
	{
		OutReason = LOCTEXT("BadCallsign", "Use 3–16 letters, digits or spaces");
		return false;
	}

	// The new game resets the profile (OnNewGame), so the callsign is set after it.
	if (!Saves->CreateNewGame(SlotIndex, Name))
	{
		OutReason = LOCTEXT("CreateFailed", "The game could not be created.");
		return false;
	}
	Profile->SetCallsign(FText::FromString(Name));
	UE_LOG(LogStartechSave, Log, TEXT("New game '%s' in slot %d."), *Name, SlotIndex);
	OpenGameMap();
	return true;
}

bool UStartechGameFlowSubsystem::LoadGame(const int32 SlotIndex, FText& OutReason)
{
	USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	if (!IsVisibleSlot(SlotIndex) || !Saves)
	{
		OutReason = LOCTEXT("NoSlotLoad", "That slot can't be loaded.");
		return false;
	}
	if (!Saves->LoadSlot(SlotIndex, OutReason))
	{
		return false;
	}
	OpenGameMap();
	return true;
}

bool UStartechGameFlowSubsystem::ContinueGame(FText& OutReason)
{
	const int32 Slot = FindMostRecentSlot();
	if (Slot == INDEX_NONE)
	{
		OutReason = LOCTEXT("NothingToContinue", "There is no saved game.");
		return false;
	}
	return LoadGame(Slot, OutReason);
}

bool UStartechGameFlowSubsystem::DeleteSlot(const int32 SlotIndex)
{
	USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	return IsVisibleSlot(SlotIndex) && Saves && Saves->DeleteSlot(SlotIndex);
}

void UStartechGameFlowSubsystem::QuitGame()
{
	UWorld* const World = GetGameInstance()->GetWorld();
	UKismetSystemLibrary::QuitGame(World, World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr,
		EQuitPreference::Quit, false);
}

void UStartechGameFlowSubsystem::QuitToMainMenu()
{
	SaveAndCloseForQuit();

	UWorld* const World = GetGameInstance()->GetWorld();
	if (World)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
	// The music outlives the map: it fades out across the load, and the menu's playlist follows.
	if (UStartechMusicSubsystem* const Music = GetGameInstance()->GetSubsystem<UStartechMusicSubsystem>())
	{
		Music->FadeOut();
	}
	const TSoftObjectPtr<UWorld>& MainMenuMap = UStartechSettings::Get().MainMenuMap;
	if (MainMenuMap.IsNull())
	{
		UE_LOG(LogStartechSave, Error, TEXT("UStartechSettings::MainMenuMap is not set; quitting the game instead."));
		QuitGame();
		return;
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), MainMenuMap);
}

void UStartechGameFlowSubsystem::QuitGameFromPlay()
{
	SaveAndCloseForQuit();
	QuitGame();
}

void UStartechGameFlowSubsystem::SaveAndCloseForQuit()
{
	USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	if (!Saves || !Saves->HasActiveSlot())
	{
		return;
	}
	static const FName QuitReason(TEXT("Quit"));
	if (!Saves->SaveNow(QuitReason))
	{
		UE_LOG(LogStartechSave, Error, TEXT("The quit save failed; the slot keeps its previous save."));
	}
	Saves->CloseActiveSlot();
}

bool UStartechGameFlowSubsystem::IsVisibleSlot(const int32 SlotIndex)
{
	return SlotIndex >= 0 && SlotIndex < UStartechSettings::Get().VisibleSaveSlots;
}

void UStartechGameFlowSubsystem::OpenGameMap()
{
	const TSoftObjectPtr<UWorld>& GameMap = UStartechSettings::Get().GameMap;
	if (GameMap.IsNull())
	{
		UE_LOG(LogStartechSave, Error, TEXT("UStartechSettings::GameMap is not set; the game can't start."));
		return;
	}

	// The menu music fades out across the load; the home sector's playlist follows when it is ready.
	if (UStartechMusicSubsystem* const Music = GetGameInstance()->GetSubsystem<UStartechMusicSubsystem>())
	{
		Music->FadeOut();
	}

	// The menu's UI-only input mode makes the game viewport ignore input, and the viewport outlives the travel:
	// hand input back to the game before leaving, so the game map never inherits it.
	if (APlayerController* const PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		FStartechInputState::SetGameOnly(*PC);
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), GameMap);
}

#undef LOCTEXT_NAMESPACE
