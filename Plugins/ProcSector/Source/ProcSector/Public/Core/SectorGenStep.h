#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/SectorTypes.h"
#include "SectorGenStep.generated.h"

class AActor;
class UWorld;

/**
 * Everything a generation step may read while building a sector.
 * Filled by the director once per regeneration and passed to every step unchanged.
 */
struct FSectorGenContext
{
	/** World being generated into. */
	UWorld* World = nullptr;

	/** Sector the player is leaving; NAME_None for the very first generation. */
	FName PreviousSectorId;

	/** Copy of the catalog entry of the sector being generated. */
	FSectorDefinition Definition;

	/** Final environment parameters, already randomised by the seed. */
	FEnvironmentParams Params;

	/** Copy of the changes the player made to this sector on earlier visits. */
	FSectorState State;

	/** Seed in effect for this sector (catalog seed plus any reroll salt). */
	int32 Seed = 0;
};

/**
 * Base class for one stage of sector generation (Strategy pattern).
 * The director calls Clear on every step, then for each step in order: Begin once,
 * and TickSlice every frame until it returns true. A step has exactly one concern,
 * such as "the asteroid field" or "the jump gates".
 */
UCLASS(Abstract)
class PROCSECTOR_API USectorGenStep : public UObject
{
	GENERATED_BODY()

public:
	/** Short name shown in the debug overlay and timing list. */
	virtual FName GetStepName() const;

	/** Called once before the first TickSlice. Resolve actors and prepare work here; do nothing slow. */
	virtual void Begin(const FSectorGenContext& Context);

	/**
	 * Does a slice of work and returns true when the step is finished.
	 * Long steps must check FPlatformTime::Seconds against DeadlineSeconds and return false to continue next frame.
	 */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds);

	/** Removes whatever this step generated for the previous sector. Must be safe to call before any Begin. */
	virtual void Clear(UWorld& World);

	/**
	 * Asks the step whether it owns Actor and, if so, to remove it from the sector.
	 * Returns true when handled. OutSpawnId and bOutRemember tell the director what to record.
	 */
	virtual bool TryConsumeActor(AActor& Actor, int32& OutSpawnId, bool& bOutRemember);

	/** Called every frame while the sector is playable (director phase Idle). For steps that maintain the live sector, such as respawns. Default: does nothing. */
	virtual void TickLive(UWorld& World, float DeltaSeconds);

	/** Number of live actors of ActorClass (or a subclass) this step placed. Default: 0. */
	virtual int32 CountLiveActors(const UClass* ActorClass) const;

	/** Number of consumed actors this step will bring back during the current visit. Default: 0. */
	virtual int32 CountPendingRespawns() const;

protected:
	/** Mixes the sector seed with a per-step constant so steps do not share a random sequence. */
	static int32 DeriveSeed(const int32 SectorSeed, const uint32 StepSalt);
};
