#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Death/HullFlash.h"
#include "Population/Poolable.h"
#include "EnemyChargeComponent.generated.h"

class AChargedShotProjectile;
class AStartechEnemyShip;
class UNiagaraComponent;

/**
 * The ranged enemy's charged shot (subobject ChargedShot on AStartechEnemyShip); the brain decides when to start one.
 *
 * BeginCharge starts the telegraph (FEnemyBehaviourBalance::ChargeTelegraphSeconds): the hull glows and pulses faster,
 * the charge effect and sound play at the muzzle, and the HUD warns the player (UChargedShotSubsystem lists the charge).
 * It also records where the target is and how it is moving. When the telegraph ends it fires one projectile at where
 * that motion would have taken the target by the time the shot arrives: a target that keeps doing the same thing is
 * hit, and one that changes direction or speed during the telegraph or the flight is missed. No homing.
 *
 * CancelCharge (death, giving up, retreating) ends it without firing. A shot already flying keeps flying.
 */
UCLASS(ClassGroup = (Startech))
class STARTECH_API UEnemyChargeComponent : public UActorComponent, public IPoolable
{
	GENERATED_BODY()

public:
	UEnemyChargeComponent();

	/** Starts a telegraph against Target with the owner's numbers. False (nothing starts) if already charging. */
	bool BeginCharge(const AActor& Target);

	/** Ends a telegraph without firing. Safe when not charging. */
	void CancelCharge();

	bool IsCharging() const { return bCharging; }

	/** 0 at the start of the telegraph, 1 at the shot. */
	float GetProgress() const;

	/** The telegraph's pulse, 0 to 1, quickening as the shot nears: the glow's rhythm, for the HUD warning to match. */
	float GetPulse() const { return bCharging ? 0.5f - 0.5f * FMath::Cos(PulsePhase * UE_TWO_PI) : 0.f; }

	/** Seconds until the shot (0 when not charging). */
	float GetTimeLeft() const;

	const AActor* GetTarget() const { return Target.Get(); }

	/** Where shots leave: between the laser mounts, or the actor's location. */
	FVector GetMuzzleLocation() const;

	/** Where the shot would go if it fired now: the target's position at the start, moved on by its velocity then. */
	FVector GetAimPoint() const;

	/** Each telegraph start, and each shot (with its projectile, null if the pool had none). For tests. */
	FSimpleMulticastDelegate OnChargeStarted;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChargeFired, AChargedShotProjectile* /* Projectile */);
	FOnChargeFired OnChargeFired;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReleasedToPool_Implementation() override;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void Fire();

	/** Clears the telegraph: glow, effect, registration and tick. */
	void EndCharge();

	AStartechEnemyShip* GetShip() const;

	bool bCharging = false;
	float Elapsed = 0.f;
	float Telegraph = 0.f;

	TWeakObjectPtr<const AActor> Target;
	FVector TargetStart = FVector::ZeroVector;
	FVector TargetVelocity = FVector::ZeroVector;

	/** Glow phase in cycles, advanced at the current pulse rate. */
	float PulsePhase = 0.f;

	UPROPERTY(Transient)
	FHullFlash Glow;

	TWeakObjectPtr<UNiagaraComponent> ChargeEffect;
};
