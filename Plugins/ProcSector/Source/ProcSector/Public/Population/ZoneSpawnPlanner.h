#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Data/SectorTypes.h"

class AActor;
struct FRandomStream;

/** The respawn and spacing settings of one spawn entry, copied so a spawn knows its rules after the plan is gone. */
struct FZoneSpawnRules
{
	/** Seconds after consumption before the spawn respawns; 0 means no respawn during the visit. */
	float RespawnDelay = 0.0f;

	/** Smallest distance from the consumed spot to the respawn point (cm). */
	float RespawnOffsetMin = 0.0f;

	/** Largest distance from the consumed spot to the respawn point (cm). */
	float RespawnOffsetMax = 0.0f;

	/** A respawn never appears closer than this to the player (cm). */
	float MinDistanceFromPlayer = 0.0f;

	/** Minimum distance to any other spawn of the sector (cm); 0 disables it. */
	float MinSpacing = 0.0f;

	/** Copies the rules of a spawn entry. Negative values become 0 and RespawnOffsetMax is raised to RespawnOffsetMin. */
	static FZoneSpawnRules FromEntry(const FZoneSpawnEntry& Entry);

	/** True when consumed spawns come back during the visit. */
	bool Respawns() const { return RespawnDelay > 0.0f; }
};

/** One actor the population step should place. */
struct FPlannedSpawn
{
	/** Position of this spawn in the plan. Stable for a given seed, so it identifies the spawn across visits. */
	int32 SpawnId = INDEX_NONE;

	/** Class to take from the pool. */
	TSubclassOf<AActor> ActorClass;

	/** Where to place the actor. */
	FTransform Transform = FTransform::Identity;

	/** From the spawn entry: whether consuming it must be remembered. Always false for an entry that respawns. */
	bool bRememberWhenConsumed = false;

	/** Index of the zone (in FEnvironmentParams::Zones) the spawn belongs to, so a respawn stays in that zone. */
	int32 ZoneIndex = INDEX_NONE;

	/** Respawn and spacing settings copied from the spawn entry. */
	FZoneSpawnRules Rules;
};

/**
 * Decides what spawns where. It only produces lists and points; it never touches the world.
 * Keeping the decision separate from the spawning makes the layout testable and repeatable.
 */
class PROCSECTOR_API FZoneSpawnPlanner
{
public:
	/**
	 * Appends one planned spawn per actor to OutPlan, for every entry of every zone, using the seed.
	 * PlayAreaExtent is the half-size of the sector's playable box; zone fractions are multiplied by it per axis.
	 * A point of an entry with MinSpacing above 0 that lands closer than that to a point already in the plan is
	 * redrawn, at most MaxSpacingRedraws times, from a separate stream; the count and yaw draws are untouched,
	 * so entries without spacing keep exactly the layout they had before spacing existed.
	 */
	static void BuildPlan(const TArray<FZoneSpawnTable>& Zones, const FVector& PlayAreaExtent, const int32 Seed, TArray<FPlannedSpawn>& OutPlan);

	/**
	 * Picks where a consumed spawn comes back, trying at most MaxRespawnTries random points from the stream.
	 * A point is RespawnOffsetMin to RespawnOffsetMax from ConsumedLocation, with its height drawn inside the zone's
	 * height band. It is accepted only if it lies inside the zone's ring and height and inside the play-area box,
	 * at least MinDistanceFromPlayer from PlayerLocation, and at least MinSpacing from every OccupiedLocations entry.
	 * Returns false, leaving OutLocation unchanged, when no try is accepted.
	 */
	static bool ChooseRespawnPoint(FRandomStream& Stream, const FZoneSpawnTable& Zone, const FVector& PlayAreaExtent,
		const FZoneSpawnRules& Rules, const FVector& ConsumedLocation, const FVector& PlayerLocation,
		const TArray<FVector>& OccupiedLocations, FVector& OutLocation);

	/** Most redraws of one planned point that is too close to another spawn; the last draw is then accepted. */
	static constexpr int32 MaxSpacingRedraws = 8;

	/** Most candidate points tried per respawn attempt. */
	static constexpr int32 MaxRespawnTries = 8;

private:
	/** Constant mixed into the plan seed for the spacing redraw stream, kept apart from the layout stream. */
	static constexpr uint32 SpacingSeedSalt = 0x5BAC1E27u;

	/** True when Location is at least MinSpacing from every point in Others. */
	static bool IsFarEnough(const FVector& Location, const TArray<FVector>& Others, const float MinSpacing);
};
