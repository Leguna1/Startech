#pragma once

#include "CoreMinimal.h"
#include "Data/SectorTypes.h"

/**
 * Remembers what the player changed in each sector (mined asteroids, consumed spawns).
 * Its only job is storage; the director decides when to record and when to read.
 * To add save games, serialise the array returned by GetAllStates.
 */
class PROCSECTOR_API FSectorStateStore
{
public:
	/** Returns the stored state for a sector, or nullptr when nothing was recorded. */
	const FSectorState* Find(const FName SectorId) const;

	/** Returns a copy of the sector's state, or an empty state carrying the id. */
	FSectorState GetCopy(const FName SectorId) const;

	/** Records that an asteroid was removed. Duplicate ids are ignored. */
	void RecordAsteroidRemoved(const FName SectorId, const int32 AsteroidId);

	/** Records that a remembered spawn was consumed. Duplicate ids are ignored. */
	void RecordSpawnConsumed(const FName SectorId, const int32 SpawnId);

	/** Forgets everything, for example after seeds are rerolled and ids no longer match. */
	void Reset();

	/** Read-only access to every stored state, for saving. */
	const TArray<FSectorState>& GetAllStates() const;

	/** Replaces every stored state, for loading. */
	void SetAllStates(const TArray<FSectorState>& InStates);

private:
	/** Returns the sector's state, creating an empty one when missing. */
	FSectorState& FindOrAdd(const FName SectorId);

	/** One entry per sector that has changes. Searched linearly. */
	TArray<FSectorState> States;
};
