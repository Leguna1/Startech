#include "Save/StartechSaveParticipantFactory.h"

#include "Save/ArsenalSaveParticipant.h"
#include "Save/CargoSaveParticipant.h"
#include "Save/ProfileSaveParticipant.h"
#include "Save/QuestsSaveParticipant.h"
#include "Save/SectorsSaveParticipant.h"
#include "Save/UpgradesSaveParticipant.h"
#include "Save/WalletSaveParticipant.h"
#include "StartechShipPawn.h"

TArray<UObject*> FStartechSaveParticipantFactory::CreateShipParticipants(AStartechShipPawn& Ship)
{
	TArray<UObject*> Created;
	const auto Make = [&Ship, &Created](UClass* const Class)
	{
		UShipSaveParticipantBase* const Participant = NewObject<UShipSaveParticipantBase>(&Ship, Class);
		Participant->Init(Ship);
		Created.Add(Participant);
	};
	Make(UWalletSaveParticipant::StaticClass());
	Make(UCargoSaveParticipant::StaticClass());
	Make(UArsenalSaveParticipant::StaticClass());
	return Created;
}

UProfileSaveParticipant* FStartechSaveParticipantFactory::CreateProfile(UObject& Outer, UPlayerProfileSubsystem& Profile)
{
	UProfileSaveParticipant* const Participant = NewObject<UProfileSaveParticipant>(&Outer);
	Participant->Init(Profile);
	return Participant;
}

UQuestsSaveParticipant* FStartechSaveParticipantFactory::CreateQuests(UObject& Outer, UQuestSubsystem& Quests, UBountyBoardSubsystem& Board)
{
	UQuestsSaveParticipant* const Participant = NewObject<UQuestsSaveParticipant>(&Outer);
	Participant->Init(Quests, Board);
	return Participant;
}

USectorsSaveParticipant* FStartechSaveParticipantFactory::CreateSectors(UObject& Outer, USectorDirectorSubsystem& Director)
{
	USectorsSaveParticipant* const Participant = NewObject<USectorsSaveParticipant>(&Outer);
	Participant->Init(Director);
	return Participant;
}

UUpgradesSaveParticipant* FStartechSaveParticipantFactory::CreateUpgrades(UObject& Outer, UShipUpgradeSubsystem& Upgrades)
{
	UUpgradesSaveParticipant* const Participant = NewObject<UUpgradesSaveParticipant>(&Outer);
	Participant->Init(Upgrades);
	return Participant;
}
