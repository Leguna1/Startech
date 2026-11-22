#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Population/Poolable.h"
#include "TeamInterface.h"
#include "ChargedShotProjectile.generated.h"

class UNiagaraComponent;

/** Everything a charged shot needs at launch. */
struct FChargedShotLaunch
{
	/** The ship that fired it: ignored by the sweep, never damaged by it, and credited with the damage. */
	TWeakObjectPtr<AActor> Shooter;
	ETeam Team = ETeam::Neutral;
	FVector Start = FVector::ZeroVector;
	/** Unit direction. */
	FVector Direction = FVector::ForwardVector;
	float Speed = 12500.f;
	float Damage = 0.f;
	float Radius = 150.f;
	float Lifetime = 2.f;
};

/**
 * The ranged enemy's charged shot: a slow, visible projectile on a straight line, with no homing. Each tick it sweeps a
 * sphere along its path on the shot channel (ECC_Visibility), so the player, an asteroid or anything else that blocks
 * shots stops it (wrecks ignore shots, so it flies through them). It damages what it hits unless that is on the
 * shooter's team, then ends; it also ends when its lifetime runs out. Pooled: taken and returned through
 * UChargedShotSubsystem, never destroyed.
 */
UCLASS()
class STARTECH_API AChargedShotProjectile : public AActor, public IPoolable
{
	GENERATED_BODY()

public:
	AChargedShotProjectile();

	/** Starts flying from Launch.Start. */
	void Launch(const FChargedShotLaunch& InLaunch);

	virtual void Tick(float DeltaSeconds) override;

	bool IsFlying() const { return bFlying; }
	const FChargedShotLaunch& GetLaunch() const { return LaunchData; }

	/** Seconds since launch. */
	float GetAge() const { return Age; }

	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReleasedToPool_Implementation() override;

private:
	/** Stops flying: impact effect (with a hit), the ended event, then back to the pool. */
	void End(const FHitResult* Hit);

	/** Stops the flight effect and clears the flight, without events. */
	void ClearFlight();

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USceneComponent> Root;

	/** The flight effect (UStartechSettings::ChargedShot.ProjectileFX), if any. */
	TWeakObjectPtr<UNiagaraComponent> Trail;

	FChargedShotLaunch LaunchData;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	bool bFlying = false;
};
