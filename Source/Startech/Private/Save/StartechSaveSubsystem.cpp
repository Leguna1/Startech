#include "Save/StartechSaveSubsystem.h"

#include "Bounties/BountyBoardSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PlayerProfileSubsystem.h"
#include "Runtime/QuestSubsystem.h"
#include "Runtime/SaveSubsystem.h"
#include "Save/ProfileSaveParticipant.h"
#include "Save/QuestsSaveParticipant.h"
#include "Save/UpgradesSaveParticipant.h"
#include "Upgrades/ShipUpgradeSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveParticipantFactory.h"
#include "SaveFrameworkSettings.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY(LogStartechSave);

UStartechSaveSubsystem* UStartechSaveSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UStartechSaveSubsystem>() : nullptr;
}

void UStartechSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	USaveSubsystem* const Saves = Collection.InitializeDependency<USaveSubsystem>();
	UPlayerProfileSubsystem* const Profile = Collection.InitializeDependency<UPlayerProfileSubsystem>();
	UQuestSubsystem* const Quests = Collection.InitializeDependency<UQuestSubsystem>();
	UBountyBoardSubsystem* const Board = Collection.InitializeDependency<UBountyBoardSubsystem>();
	UShipUpgradeSubsystem* const Upgrades = Collection.InitializeDependency<UShipUpgradeSubsystem>();
	Super::Initialize(Collection);

	if (!Saves)
	{
		UE_LOG(LogStartechSave, Error, TEXT("No USaveSubsystem: the profile and quests will not be saved."));
		return;
	}

	if (Profile)
	{
		ProfileParticipant = FStartechSaveParticipantFactory::CreateProfile(*this, *Profile);
		Saves->RegisterParticipant(ProfileParticipant);
		Profile->OnCallsignChanged.AddUniqueDynamic(this, &UStartechSaveSubsystem::HandleCallsignChanged);
	}
	if (Quests && Board)
	{
		QuestsParticipant = FStartechSaveParticipantFactory::CreateQuests(*this, *Quests, *Board);
		Saves->RegisterParticipant(QuestsParticipant);
	}
	if (Upgrades)
	{
		UpgradesParticipant = FStartechSaveParticipantFactory::CreateUpgrades(*this, *Upgrades);
		Saves->RegisterParticipant(UpgradesParticipant);
	}
}

void UStartechSaveSubsystem::Deinitialize()
{
	if (USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>())
	{
		Saves->UnregisterParticipant(ProfileParticipant);
		Saves->UnregisterParticipant(QuestsParticipant);
		Saves->UnregisterParticipant(UpgradesParticipant);
	}
	if (UPlayerProfileSubsystem* const Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>())
	{
		Profile->OnCallsignChanged.RemoveDynamic(this, &UStartechSaveSubsystem::HandleCallsignChanged);
	}
	ProfileParticipant = nullptr;
	QuestsParticipant = nullptr;
	UpgradesParticipant = nullptr;
	Super::Deinitialize();
}

void UStartechSaveSubsystem::HandleCallsignChanged(const FText NewCallsign)
{
	if (USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>())
	{
		Saves->SetActiveDisplayName(MakeDisplayName(NewCallsign.ToString()));
	}
}

const FString UStartechSaveSubsystem::TestSlotPrefix(TEXT("[PIE] "));

int32 UStartechSaveSubsystem::GetTestSlotIndex()
{
	return USaveFrameworkSettings::Get().NumSlots - 1;
}

bool UStartechSaveSubsystem::IsTestSlotActive() const
{
	const USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	return Saves && Saves->HasActiveSlot() && Saves->GetActiveSlot() == GetTestSlotIndex();
}

void UStartechSaveSubsystem::StartTestGame()
{
	USaveSubsystem* const Saves = GetGameInstance()->GetSubsystem<USaveSubsystem>();
	const UPlayerProfileSubsystem* const Profile = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
	if (!Saves)
	{
		return;
	}

	const int32 TestSlot = GetTestSlotIndex();
	if (TestSlot < UStartechSettings::Get().VisibleSaveSlots)
	{
		UE_LOG(LogStartechSave, Warning, TEXT("SaveFramework NumSlots (%d) leaves no slot above the %d visible ones; the test game "
			"uses slot %d, which the menu shows."), USaveFrameworkSettings::Get().NumSlots, UStartechSettings::Get().VisibleSaveSlots, TestSlot);
	}

	// The profile resets to its default callsign on the new game, so name the slot after that.
	Saves->CreateNewGame(TestSlot, TestSlotPrefix);
	Saves->SetActiveDisplayName(MakeDisplayName(Profile ? Profile->GetCallsign().ToString() : FString()));
	UE_LOG(LogStartechSave, Log, TEXT("Game map started without the menu: temporary new game in test slot %d."), TestSlot);
}

FString UStartechSaveSubsystem::MakeDisplayName(const FString& Callsign) const
{
	return IsTestSlotActive() ? TestSlotPrefix + Callsign : Callsign;
}
