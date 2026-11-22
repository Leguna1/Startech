#include "Save/SectorsSaveParticipant.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"

const FName USectorsSaveParticipant::ParticipantId(TEXT("Startech.Sectors"));

void USectorsSaveParticipant::Init(USectorDirectorSubsystem& InDirector)
{
	Director = &InDirector;
}

FName USectorsSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 USectorsSaveParticipant::GetSaveVersion() const
{
	// 2: sector tiers made enemies respawn instead of being remembered (see ReadSaveData).
	return 2;
}

void USectorsSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	FStartechSectorsSave Save;
	if (const USectorDirectorSubsystem* const SectorDirector = Director.Get())
	{
		for (const FSectorState& State : SectorDirector->GetStateStore().GetAllStates())
		{
			FStartechSectorSave& Entry = Save.Sectors.AddDefaulted_GetRef();
			Entry.SectorId = State.SectorId;
			Entry.RemovedAsteroidIds = State.RemovedAsteroidIds;
			Entry.ConsumedSpawnIds = State.ConsumedSpawnIds;
		}
	}
	FSaveSerialization::StructToBytes(Save, OutData);
}

void USectorsSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechSectorsSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Sectors: unreadable record; sectors start fresh instead."));
		OnNewGame();
		return;
	}

	USectorDirectorSubsystem* const SectorDirector = Director.Get();
	if (!SectorDirector)
	{
		return;
	}

	// Version 1 remembered killed enemies; enemies now respawn, and the presets' spawn plans changed, so their old
	// spawn ids would point at other spawns. In version 1 data every consumed spawn id is an enemy: loot was never
	// remembered and stations are never consumed. Mined asteroids are kept.
	const bool bDropConsumedSpawns = SavedVersion < 2;
	int32 Dropped = 0;

	TArray<FSectorState> States;
	for (const FStartechSectorSave& Entry : Save.Sectors)
	{
		FSectorState& State = States.AddDefaulted_GetRef();
		State.SectorId = Entry.SectorId;
		State.RemovedAsteroidIds = Entry.RemovedAsteroidIds;
		if (bDropConsumedSpawns)
		{
			Dropped += Entry.ConsumedSpawnIds.Num();
		}
		else
		{
			State.ConsumedSpawnIds = Entry.ConsumedSpawnIds;
		}
	}
	if (bDropConsumedSpawns)
	{
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Sectors: version %d save; %d remembered enemy spawn(s) dropped."),
			SavedVersion, Dropped);
	}
	SectorDirector->GetMutableStateStore().SetAllStates(States);
	UE_LOG(LogStartechSave, Log, TEXT("Startech.Sectors: %d sector(s) with remembered changes."), States.Num());
}

void USectorsSaveParticipant::OnNewGame()
{
	if (USectorDirectorSubsystem* const SectorDirector = Director.Get())
	{
		SectorDirector->GetMutableStateStore().Reset();
	}
}
