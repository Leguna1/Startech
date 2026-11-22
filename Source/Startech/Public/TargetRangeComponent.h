#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetLockState.h"
#include "TargetRangeState.h"
#include "WeaponGroup.h"
#include "TargetRangeComponent.generated.h"

class UTargetableComponent;
class UTargetingComponent;

/** Broadcast when the range state against the current target changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRangeStateChanged, ETargetRangeState, NewState);

/** Broadcast when the current target moves beyond (true) or back within (false) one weapon group's range. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroupRangeChanged, EWeaponGroup, Group, bool, bOutOfRange);

/**
 * Answers one question for the player's ship: where do I stand relative to my target's range and mine?
 * Follows the owner's UTargetingComponent and, while it has a target (soft or hard), re-evaluates on a timer at
 * EvaluationRate, never on tick. The target's range is its fitted laser range (enemies get theirs from EnemyData's
 * LaserWeapon); the player's is the longest range among the weapons that can fire right now
 * (UWeaponSystemComponent::GetUsableWeaponRange).
 */
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UTargetRangeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Never ticks; evaluation runs on a timer. */
	UTargetRangeComponent();

	/** The state from the latest evaluation. OutOfRange with no target. */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	ETargetRangeState GetRangeState() const { return RangeState; }

	/**
	 * Distance from the owner to the current target's aim point in cm, computed now; 0 with no target. Cheap: the one
	 * value the HUD may read every frame.
	 */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	float GetDistanceToTarget() const;

	/** Fires only when the range state changes, including the drop to OutOfRange when the target is lost. */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnRangeStateChanged OnRangeStateChanged;

	/**
	 * True while there is a target (soft or hard) beyond Group's range (UWeaponSystemComponent::GetGroupRange), or the
	 * group has no weapon. False with no target. Evaluated with the range state, on the same timer.
	 */
	bool IsGroupOutOfRange(EWeaponGroup Group) const;

	/** Fires when a group's IsGroupOutOfRange changes, including the drop to false when the target is lost. */
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnGroupRangeChanged OnGroupRangeChanged;

protected:
	/** Binds to the owner's targeting component and picks up a target it may already have. */
	virtual void BeginPlay() override;

	/** Unbinds and stops the timer. */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Evaluations per second while a target exists. */
	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "1"))
	float EvaluationRate = 10.f;

private:
	/** Starts or stops evaluating as the targeting component's target comes and goes. */
	UFUNCTION()
	void HandleTargetChanged(UTargetableComponent* NewTarget, ETargetLockState NewState);

	/** Computes the state for the current target and applies it. */
	void Evaluate();

	/** The state for the current target, computed now. */
	ETargetRangeState ComputeRangeState() const;

	/** One weapon group's out-of-range flag. */
	struct FGroupRange
	{
		/** The group. */
		EWeaponGroup Group = EWeaponGroup::Laser;
		/** The target is beyond the group's range. */
		bool bOutOfRange = false;
	};

	/** Recomputes every group's flag (all false without a target) and fires OnGroupRangeChanged for each change. */
	void EvaluateGroups(bool bHasTarget);

	/** One entry per weapon group. */
	TArray<FGroupRange> GroupRanges;

	/** Stores NewState and fires OnRangeStateChanged if it differs. */
	void SetRangeState(ETargetRangeState NewState);

	/** The target being rated, or null. */
	const UTargetableComponent* GetTarget() const;

	/** The owner's targeting component, found at BeginPlay. */
	TWeakObjectPtr<UTargetingComponent> Targeting;

	/** Running while a target exists. */
	FTimerHandle EvaluationTimer;

	/** Last state broadcast. */
	ETargetRangeState RangeState = ETargetRangeState::OutOfRange;
};
