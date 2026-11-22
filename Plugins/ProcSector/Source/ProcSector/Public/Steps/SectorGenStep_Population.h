#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "Population/ZoneSpawnPlanner.h"
#include "SectorGenStep_Population.generated.h"

class UActorPoolSubsystem;

/** An actor this step placed in the level, with the bookkeeping needed to consume, respawn or release it. */
struct FSpawnedSectorActor
{
	/** The pooled actor. */
	TWeakObjectPtr<AActor> Actor;

	/** Stable id of the spawn within the sector, from the spawn plan. A respawn keeps the id of the spawn it replaces. */
	int32 SpawnId = INDEX_NONE;

	/** From the spawn entry: whether consuming it must be remembered. Always false for an entry that respawns. */
	bool bRememberWhenConsumed = false;

	/** Zone the spawn belongs to, so its respawn stays in that zone. */
	int32 ZoneIndex = INDEX_NONE;

	/** Respawn and spacing settings copied from the spawn entry. */
	FZoneSpawnRules Rules;
};

/** A consumed spawn waiting to come back during the current visit. */
struct FPendingRespawn
{
	/** Stable id of the consumed spawn; the respawn is tracked under the same id. */
	int32 SpawnId = INDEX_NONE;

	/** Class to take from the pool. */
	TSubclassOf<AActor> ActorClass;

	/** Zone the respawn must appear in. */
	int32 ZoneIndex = INDEX_NONE;

	/** Respawn and spacing settings copied from the spawn entry. */
	FZoneSpawnRules Rules;

	/** Where the spawn was when it was consumed; the respawn appears RespawnOffsetMin to RespawnOffsetMax from it. */
	FVector ConsumedLocation = FVector::ZeroVector;

	/** World time (UWorld::GetTimeSeconds) from which the respawn is attempted. */
	double DueTime = 0.0;

	/** Failed attempts so far; part of the respawn's seed, so each retry tries different points. */
	int32 AttemptCounter = 0;
};

/**
 * Generation step that fills the spawn zones with pooled actors (enemies, loot, stations, NPCs).
 * It executes a plan made by FZoneSpawnPlanner a few actors per frame and skips
 * anything the sector state says was already consumed. While the sector is playable it brings
 * consumed spawns back after their entry's RespawnDelay, near where they were consumed and away from the player.
 */
UCLASS()
class PROCSECTOR_API USectorGenStep_Population : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "Population". */
	virtual FName GetStepName() const override;

	/** Builds the seeded spawn plan for the sector, with zones scaled by its PlayAreaExtent, and keeps what respawns need. */
	virtual void Begin(const FSectorGenContext& Context) override;

	/** Spawns planned actors from the pool until the plan is done or the deadline passes. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

	/** Returns every actor this step placed to the pool and drops all pending respawns. */
	virtual void Clear(UWorld& World) override;

	/**
	 * If Actor was placed by this step: releases it to the pool and reports its spawn id.
	 * If its entry has a RespawnDelay, queues a respawn after releasing it.
	 */
	virtual bool TryConsumeActor(AActor& Actor, int32& OutSpawnId, bool& bOutRemember) override;

	/** Brings back every due respawn that finds an accepted point; pushes the others back by RetryDelaySeconds. */
	virtual void TickLive(UWorld& World, float DeltaSeconds) override;

	/** Number of tracked live actors whose class is ActorClass or a subclass of it. */
	virtual int32 CountLiveActors(const UClass* ActorClass) const override;

	/** Number of queued respawns. */
	virtual int32 CountPendingRespawns() const override;

private:
	/** Takes an actor from the pool and tracks it under the spawn id. Returns false if the pool gave nothing. */
	bool SpawnTracked(UActorPoolSubsystem& Pool, const TSubclassOf<AActor> ActorClass, const FTransform& Transform,
		const int32 SpawnId, const bool bRememberWhenConsumed, const int32 ZoneIndex, const FZoneSpawnRules& Rules);

	/** Tries to place one due respawn. Returns true when it was spawned and can leave the queue. */
	bool TryRespawn(UActorPoolSubsystem& Pool, const FPendingRespawn& Pending, const FVector& PlayerLocation);

	/** Where respawns keep their distance from: the first player's pawn, or the view location without a pawn. */
	static FVector GetPlayerLocation(const UWorld& World);

	/** Constant mixed into the sector seed for this step's random sequence. */
	static constexpr uint32 SeedSalt = 0x50B0C1A7u;

	/** Constant mixed into the sector seed for respawn points, which depend on the player and are not part of the layout. */
	static constexpr uint32 RespawnSeedSalt = 0x2E5B4A3Du;

	/** Seconds a respawn waits before trying again when none of its candidate points was accepted. */
	static constexpr double RetryDelaySeconds = 5.0;

	/** The spawn plan being executed. */
	TArray<FPlannedSpawn> Plan;

	/** Index of the next plan entry to spawn. */
	int32 PlanCursor = 0;

	/** Actors currently placed in the level by this step. */
	TArray<FSpawnedSectorActor> Spawned;

	/** Consumed spawns waiting to come back during this visit. */
	TArray<FPendingRespawn> PendingRespawns;

	/** Zones of the current sector, so a respawn knows its ring and height. */
	TArray<FZoneSpawnTable> Zones;

	/** Half-size of the current sector's play area. */
	FVector PlayAreaExtent = FVector::ZeroVector;

	/** Seed of the current sector, for the respawn streams. */
	int32 SectorSeed = 0;

	/** Keeps the classes of the sector's spawns referenced, for generation and for respawns, so garbage collection cannot unload them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UClass>> LoadedClasses;
};
