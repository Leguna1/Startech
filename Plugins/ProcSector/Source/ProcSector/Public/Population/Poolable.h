#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Poolable.generated.h"

/** Reflection stub for IPoolable. Not used directly. */
UINTERFACE(BlueprintType)
class PROCSECTOR_API UPoolable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on an actor or component that needs to reset itself when reused by the pool.
 * A pooled actor is never constructed again, so BeginPlay runs only once: anything that
 * must restart per use (health, AI state, timers, velocity) belongs in OnAcquiredFromPool.
 */
class PROCSECTOR_API IPoolable
{
	GENERATED_BODY()

public:
	/** Called after the actor was activated and placed. Reset per-use state here. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ProcSector|Pool")
	void OnAcquiredFromPool();

	/** Called just before the actor is hidden and stored. Stop timers, effects and AI here. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ProcSector|Pool")
	void OnReleasedToPool();
};
