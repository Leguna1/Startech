#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ChargedShotSubsystem.generated.h"

class AChargedShotProjectile;
class UEnemyChargeComponent;
class USectorDirectorSubsystem;
struct FChargedShotLaunch;

/** A charge telegraph started (native, every world). The combat log and tests listen. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnyChargeStarted, const UEnemyChargeComponent& /* Charge */);

/** A charged shot left the muzzle. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnyChargedShotFired, const AChargedShotProjectile& /* Projectile */);

/** A charged shot ended: HitActor is what it hit (null when it ran out of time), bDamaged whether it dealt damage. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAnyChargedShotEnded, const AChargedShotProjectile& /* Projectile */,
	AActor* /* HitActor */, bool /* bDamaged */);

/**
 * Owns the charged shots of a world: takes projectiles from the pool (Fire) and gives them back (Release), and returns
 * every one still flying when a sector transition starts. Also the registry of charges in progress, which the HUD reads
 * for its warning (GetActiveCharges, IsCharging).
 */
UCLASS()
class STARTECH_API UChargedShotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UChargedShotSubsystem* Get(const UObject* WorldContextObject);

	/** A pooled projectile, launched. Null without a pool. */
	AChargedShotProjectile* Fire(const FChargedShotLaunch& Launch);

	/** Back to the pool. Safe for one already returned. */
	void Release(AChargedShotProjectile& Projectile);

	/** Projectiles in flight. */
	int32 GetFlyingCount() const;

	void RegisterCharge(UEnemyChargeComponent& Charge);
	void UnregisterCharge(const UEnemyChargeComponent& Charge);

	/** Charges whose telegraph is running. */
	TArray<const UEnemyChargeComponent*> GetActiveCharges() const;

	/** True while Ship's telegraph is running. */
	bool IsCharging(const AActor* Ship) const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	static FOnAnyChargeStarted OnAnyChargeStarted;
	static FOnAnyChargedShotFired OnAnyChargedShotFired;
	static FOnAnyChargedShotEnded OnAnyChargedShotEnded;

private:
	UFUNCTION()
	void HandleSectorTransitionStarted(FName FromSectorId, FName ToSectorId);

	TArray<TWeakObjectPtr<AChargedShotProjectile>> Flying;
	TArray<TWeakObjectPtr<UEnemyChargeComponent>> Charges;
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;
};
