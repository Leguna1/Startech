#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Population/Poolable.h"
#include "TargetLockState.h"
#include "TargetableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockStateChanged, ETargetLockState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLost, UTargetableComponent*, Target);

/** Broadcast whenever a target's targetable flag changes, in either direction. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetableChanged, UTargetableComponent*, Target, bool, bTargetable);

// Marks an actor as something the player can lock on to. Place it where the lock should aim.
// A pooled actor stays valid while stored, so the targetable flag, not IsValid, says whether it can be targeted:
// it goes off on death and on release to the pool, and back on when the pool hands the actor out again.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UTargetableComponent : public USceneComponent, public IPoolable
{
	GENERATED_BODY()

public:
	FVector GetAimLocation() const { return GetComponentLocation(); }

	bool IsTargetable() const { return bTargetable; }

	// The owning actor calls SetTargetable(false) when it dies; that releases any lock on it.
	void SetTargetable(bool bNewTargetable);

	/** Pool hook: targetable again (back to its value at BeginPlay), broadcast through OnTargetableChanged. */
	virtual void OnAcquiredFromPool_Implementation() override;

	/** Pool hook: not targetable, which releases any lock on the stored actor. */
	virtual void OnReleasedToPool_Implementation() override;

	/** Fires on every change of the targetable flag. Anything holding a reference to a target should react to it. */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTargetableChanged OnTargetableChanged;

	// Fired once per loss: when targetable goes true -> false, or in EndPlay while still targetable.
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTargetLost OnTargetLost;

	// Fired by the targeting component when this target gains, changes or loses a lock.
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnLockStateChanged OnLockStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	// Read-only to Blueprint, so every change goes through SetTargetable and is announced.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (AllowPrivateAccess = "true"))
	bool bTargetable = true;

	/** The authored targetable value, captured at BeginPlay and restored each time the pool hands the actor out. */
	bool bTargetableAtStart = true;
};
