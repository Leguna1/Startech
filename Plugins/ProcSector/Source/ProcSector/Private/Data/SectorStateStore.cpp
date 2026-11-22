#include "Data/SectorStateStore.h"

const FSectorState* FSectorStateStore::Find(const FName SectorId) const
{
	return States.FindByPredicate([SectorId](const FSectorState& State)
	{
		return State.SectorId == SectorId;
	});
}

FSectorState FSectorStateStore::GetCopy(const FName SectorId) const
{
	if (const FSectorState* Existing = Find(SectorId))
	{
		return *Existing;
	}

	FSectorState Empty;
	Empty.SectorId = SectorId;
	return Empty;
}

void FSectorStateStore::RecordAsteroidRemoved(const FName SectorId, const int32 AsteroidId)
{
	FindOrAdd(SectorId).RemovedAsteroidIds.AddUnique(AsteroidId);
}

void FSectorStateStore::RecordSpawnConsumed(const FName SectorId, const int32 SpawnId)
{
	FindOrAdd(SectorId).ConsumedSpawnIds.AddUnique(SpawnId);
}

void FSectorStateStore::Reset()
{
	States.Reset();
}

const TArray<FSectorState>& FSectorStateStore::GetAllStates() const
{
	return States;
}

void FSectorStateStore::SetAllStates(const TArray<FSectorState>& InStates)
{
	States = InStates;
}

FSectorState& FSectorStateStore::FindOrAdd(const FName SectorId)
{
	FSectorState* Existing = States.FindByPredicate([SectorId](const FSectorState& State)
	{
		return State.SectorId == SectorId;
	});

	if (Existing != nullptr)
	{
		return *Existing;
	}

	FSectorState& Added = States.AddDefaulted_GetRef();
	Added.SectorId = SectorId;
	return Added;
}
