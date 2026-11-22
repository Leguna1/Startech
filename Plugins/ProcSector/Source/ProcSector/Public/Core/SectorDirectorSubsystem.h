#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "Core/SectorGenStep.h"
#include "Data/SectorStateStore.h"
#include "Data/SectorTypes.h"
#include "SectorDirectorSubsystem.generated.h"

class USectorCatalog;

/** Broadcast when a sector has finished generating and is playable. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSectorReady, FName, SectorId);

/** Broadcast when regeneration starts, before anything is cleared. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSectorTransitionStarted, FName, FromSectorId, FName, ToSectorId);

/**
 * The single entry point for changing sectors (Facade pattern).
 * It owns the regeneration cycle: clear the old sector, run each generation step within a
 * per-frame time budget, then announce the new sector. It does not know what any step builds;
 * it only sequences them. One instance exists per game world and is fetched with
 * GetWorld()->GetSubsystem<USectorDirectorSubsystem>().
 */
UCLASS()
class PROCSECTOR_API USectorDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Generates the start sector when the settings ask for automatic generation. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Advances the regeneration cycle by one time-budgeted slice. While idle with a current sector, calls TickLive on every step instead. */
	virtual void Tick(float DeltaTime) override;

	/** Stat id required by the tickable interface. */
	virtual TStatId GetStatId() const override;

	/** Starts regenerating the level as the given sector. Returns false if busy or the sector is unknown. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector")
	bool RequestSector(FName SectorId);

	/** Regenerates the current sector in place, for example after changing settings. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector")
	bool RegenerateCurrentSector();

	/** Picks a new random salt for every seed, forgets all sector state and regenerates. A debugging aid. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector")
	void RerollSeeds();

	/** True while a regeneration is in progress. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	bool IsBusy() const;

	/** Current phase of the regeneration cycle. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	ESectorPhase GetPhase() const;

	/** Id of the sector that is currently playable. NAME_None before the first generation finishes. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	FName GetCurrentSectorId() const;

	/** Id of the sector the player came from. NAME_None after the first generation. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	FName GetPreviousSectorId() const;

	/** Seed in effect for the current or pending sector. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	int32 GetCurrentSeed() const;

	/** Half-size of the current sector's playable box, centred on the level origin. During a regeneration, the sector being generated. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	FVector GetPlayAreaExtent() const;

	/** Environment parameters of the current or pending sector. */
	const FEnvironmentParams& GetCurrentParams() const;

	/** The catalog in use, created or loaded on first need. Never null after the first call. */
	const USectorCatalog* GetCatalog();

	/** The catalog in use, or nullptr if nothing has needed it yet. */
	const USectorCatalog* GetCatalog() const;

	/**
	 * Computes where an arriving ship should appear: just inside the gate that leads back to FromSectorId,
	 * with the gate's NormalizedLocation resolved against the current sector's PlayAreaExtent.
	 * Returns false when the current sector has no such gate, in which case the ship should stay where it is.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcSector")
	bool GetArrivalTransform(FName FromSectorId, FTransform& OutTransform) const;

	/** Stores that an asteroid of the current sector was removed, so it stays gone on later visits. */
	void RecordAsteroidRemoved(const int32 AsteroidId);

	/**
	 * Call when the player loots or destroys an actor that the sector generated.
	 * The owning step returns it to the pool and, if its spawn entry asks for it, the removal is remembered.
	 * Returns false when no step owns the actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcSector")
	bool ConsumeSpawnedActor(AActor* Actor);

	/** Number of live sector-spawned actors of ActorClass (or a subclass), summed over all steps. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	int32 CountLiveSpawns(TSubclassOf<AActor> ActorClass) const;

	/** Number of consumed sector spawns waiting to respawn during the current visit, summed over all steps. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	int32 CountPendingRespawns() const;

	/** Per-step timings of the most recent regeneration. */
	const TArray<FSectorStepTiming>& GetLastStepTimings() const;

	/** Total working time of the most recent regeneration, in milliseconds. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	float GetLastGenerationMs() const;

	/** Read-only access to the per-sector state, for saving. */
	const FSectorStateStore& GetStateStore() const;

	/** Mutable access to the per-sector state, for loading a save before the first RequestSector. */
	FSectorStateStore& GetMutableStateStore();

	/** Fires when a sector has finished generating. */
	UPROPERTY(BlueprintAssignable, Category = "ProcSector")
	FOnSectorReady OnSectorReady;

	/** Fires when regeneration starts. */
	UPROPERTY(BlueprintAssignable, Category = "ProcSector")
	FOnSectorTransitionStarted OnSectorTransitionStarted;

protected:
	/** Limits the subsystem to game and Play-In-Editor worlds. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Loads or creates the catalog and builds the step pipeline, once. */
	void EnsureInitialized();

	/** Calls Clear on every step. */
	void TearDownCurrentSector();

	/** Calls Begin on the step at StepIndex and resets its timing accumulator. */
	void BeginCurrentStep();

	/** Marks the pending sector as current, returns to Idle and broadcasts OnSectorReady. */
	void FinishGeneration();

	/** Combines a sector's catalog seed with the reroll salt. */
	int32 ResolveSeed(const FSectorDefinition& Definition) const;

	/** The catalog in use: the asset from the settings, or the built-in test catalog. */
	UPROPERTY(Transient)
	TObjectPtr<USectorCatalog> Catalog;

	/** The generation pipeline, in execution order. Created by FSectorGenStepFactory. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USectorGenStep>> Steps;

	/** Input handed to every step during the regeneration in progress. */
	FSectorGenContext Context;

	/** Remembered player changes per sector. */
	FSectorStateStore StateStore;

	/** Current phase of the cycle. */
	ESectorPhase Phase = ESectorPhase::Idle;

	/** Sector that is currently playable. */
	FName CurrentSectorId;

	/** Sector the player came from. */
	FName PreviousSectorId;

	/** Sector being generated. */
	FName PendingSectorId;

	/** Index of the step currently running. */
	int32 StepIndex = 0;

	/** Random offset added to every catalog seed; changed by RerollSeeds. */
	int32 SeedSalt = 0;

	/** Working time the current step has used so far, in seconds. */
	double StepAccumulatedSeconds = 0.0;

	/** Timings collected during the regeneration in progress, published when it finishes. */
	TArray<FSectorStepTiming> PendingTimings;

	/** Timings of the most recent finished regeneration. */
	TArray<FSectorStepTiming> LastTimings;

	/** Total working time of the most recent finished regeneration, in milliseconds. */
	float LastGenerationMs = 0.0f;

	/** True once EnsureInitialized has run. */
	bool bInitializedPipeline = false;
};
