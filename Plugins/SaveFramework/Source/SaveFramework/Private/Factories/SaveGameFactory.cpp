#include "Factories/SaveGameFactory.h"

#include "SaveFrameworkSaveGame.h"
#include "SaveFrameworkSettings.h"
#include "SaveParticipant.h"
#include "UObject/Package.h"

FSaveRecord FSaveGameFactory::MakeRecord(const ISaveParticipant& Participant)
{
	FSaveRecord Record;
	Record.ParticipantId = Participant.GetSaveParticipantId();
	Record.Version = Participant.GetSaveVersion();
	Participant.WriteSaveData(Record.Data);
	return Record;
}

FSaveSlotHeader FSaveGameFactory::MakeHeader(const int32 SlotIndex, const FString& DisplayName, const double PlayTimeSeconds)
{
	FSaveSlotHeader Header;
	Header.SlotIndex = SlotIndex;
	Header.DisplayName = DisplayName;
	Header.PlayTimeSeconds = PlayTimeSeconds;
	Header.SavedUtc = FDateTime::UtcNow();
	Header.FrameworkVersion = USaveFrameworkSaveGame::CurrentFrameworkVersion;
	Header.ProjectVersion = USaveFrameworkSettings::Get().ProjectVersion;
	return Header;
}

USaveFrameworkSaveGame* FSaveGameFactory::MakeSaveGame(const FSaveSlotHeader& Header, const TArray<FSaveRecord>& Records)
{
	USaveFrameworkSaveGame* const SaveGame = NewObject<USaveFrameworkSaveGame>(GetTransientPackage());
	SaveGame->Header = Header;
	SaveGame->Records = Records;
	SaveGame->Seal();
	return SaveGame;
}
