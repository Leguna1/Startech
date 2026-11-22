#include "Save/ProfileSaveParticipant.h"

#include "PlayerProfileSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"

const FName UProfileSaveParticipant::ParticipantId(TEXT("Startech.Profile"));

void UProfileSaveParticipant::Init(UPlayerProfileSubsystem& InProfile)
{
	Profile = &InProfile;
}

FName UProfileSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 UProfileSaveParticipant::GetSaveVersion() const
{
	// 2: the highest sector tier visited.
	return 2;
}

void UProfileSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	FStartechProfileSave Save;
	if (const UPlayerProfileSubsystem* const Subsystem = Profile.Get())
	{
		Save.Callsign = Subsystem->GetCallsign().ToString();
		Save.HighestTierVisited = Subsystem->GetHighestTierVisited();
	}
	FSaveSerialization::StructToBytes(Save, OutData);
}

void UProfileSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechProfileSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Profile: unreadable record; default callsign instead."));
		OnNewGame();
		return;
	}
	if (UPlayerProfileSubsystem* const Subsystem = Profile.Get())
	{
		Subsystem->SetCallsign(FText::FromString(Save.Callsign));
		// Version 1 had no tiers: the pilot had only seen Terra and Rust, and Rust's first-visit notice is new.
		Subsystem->SetHighestTierVisited(SavedVersion < 2 ? 1 : Save.HighestTierVisited);
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Profile: callsign '%s', highest tier visited %d (version %d)."),
			*Subsystem->GetCallsign().ToString(), Subsystem->GetHighestTierVisited(), SavedVersion);
	}
}

void UProfileSaveParticipant::OnNewGame()
{
	if (UPlayerProfileSubsystem* const Subsystem = Profile.Get())
	{
		Subsystem->SetCallsign(FText::GetEmpty());
		Subsystem->SetHighestTierVisited(1);
	}
}
