#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "ActorPoolSubsystem.generated.h"

class AActor;

/** All stored and in-use actors of one class. */
USTRUCT()
struct FActorPoolBucket
{
	GENERATED_BODY()

	/** The exact class this bucket holds. */
	UPROPERTY()
	TObjectPtr<UClass> ActorClass = nullptr;

	/** Deactivated actors waiting to be reused. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Free;

	/** Number of actors of this class currently handed out. */
	int32 ActiveCount = 0;
};

/**
 * Object pool for actors (Object Pool pattern, and the factory for every pooled actor).
 * Acquire hands out a stored actor or spawns a new one; Release hides and stores it.
 * Nothing is destroyed between sectors, which avoids spawn cost and garbage-collection spikes.
 */
UCLASS()
class PROCSECTOR_API UActorPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Returns an active actor of the class at the transform, reusing a stored one when possible. Null if spawning fails. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector|Pool")
	AActor* Acquire(TSubclassOf<AActor> ActorClass, const FTransform& Transform);

	/** Deactivates the actor and stores it for reuse. Safe to call twice and on actors that were not pooled. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector|Pool")
	void Release(AActor* Actor);

	/** Spawns Count actors of the class straight into storage, to move spawn cost to a loading moment. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector|Pool")
	void Prewarm(TSubclassOf<AActor> ActorClass, int32 Count);

	/** Total actors currently handed out, across all classes. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Pool")
	int32 GetActiveCount() const;

	/** Total actors currently stored, across all classes. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Pool")
	int32 GetFreeCount() const;

private:
	/** Returns the bucket for the class, creating it when missing. */
	FActorPoolBucket& FindOrAddBucket(UClass* ActorClass);

	/** Spawns a brand-new actor of the class. */
	AActor* SpawnNew(UClass* ActorClass, const FTransform& Transform) const;

	/**
	 * Places the actor and turns rendering, collision and ticking back on, then notifies IPoolable.
	 * The actor's tick and each component's tick are restored to their class's bStartWithTickEnabled.
	 */
	static void Activate(AActor& Actor, const FTransform& Transform);

	/**
	 * Notifies IPoolable, then turns rendering, collision and ticking off. Every component's tick is disabled and
	 * every timer bound to the actor or one of its components is cleared, so nothing keeps running while it is stored.
	 */
	static void Deactivate(AActor& Actor);

	/** Calls the IPoolable event on the actor and on each of its components that implement it. */
	static void NotifyPoolables(AActor& Actor, const bool bAcquired);

	/** One bucket per class. Searched linearly; the number of pooled classes is small. */
	UPROPERTY()
	TArray<FActorPoolBucket> Buckets;
};
