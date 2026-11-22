#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetLockState.h"
#include "TargetingComponent.generated.h"

class UTargetableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetChanged, UTargetableComponent*, NewTarget, ETargetLockState, NewState);

/** Broadcast when a hard lock starts or ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHardLockEvent);

// Picks a soft-lock target from the centre of the player's view, and lets the player pin it as a hard lock.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Releases a hard lock, or pins the current soft-lock target.
	void ToggleHardLock();

	// Drops the current target and any lock.
	void ClearTarget();

	/** Hard-locks Target directly, as pinning it after a soft lock would (test pilots). Null clears. */
	void LockTarget(UTargetableComponent* Target);

	/** The soft- or hard-locked target, or null. */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	UTargetableComponent* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** The actor owning the current target, or null. */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetCurrentTargetActor() const;

	/** None, Soft or Hard. */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	ETargetLockState GetLockState() const { return LockState; }

	// The current target's aim location, or else a point LockRange straight ahead of the owner.
	FVector GetAimPoint() const;

	// Fired whenever the target or the lock state changes.
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTargetChanged OnTargetChanged;

	/** Fires when the lock becomes hard (normally from a soft lock on the same target), after OnTargetChanged. */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnHardLockEvent OnHardLockEngaged;

	/** Fires when a hard lock ends for any reason (released, broken, target lost), after OnTargetChanged. */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnHardLockEvent OnHardLockReleased;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	// In cm, measured from the owner.
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float LockRange = 20000.f;

	// Degrees from the centre of view that a target can be soft locked.
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float SoftLockHalfAngle = 12.f;

	// A hard lock breaks once the target is farther than LockRange times this.
	UPROPERTY(EditDefaultsOnly, Category = "Targeting")
	float HardLockBreakRangeScale = 1.2f;

private:
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;
	bool IsHardLockValid() const;
	UTargetableComponent* FindSoftTarget(const FVector& ViewLocation, const FVector& ViewDirection) const;
	void SetTarget(UTargetableComponent* NewTarget, ETargetLockState NewState);

	// Bound to the current target's OnTargetLost; releases the lock immediately.
	UFUNCTION()
	void HandleTargetLost(UTargetableComponent* LostTarget);

	TWeakObjectPtr<UTargetableComponent> CurrentTarget;
	ETargetLockState LockState = ETargetLockState::None;
};
