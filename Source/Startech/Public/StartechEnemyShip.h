#pragma once

#include "CoreMinimal.h"
#include "Population/Poolable.h"
#include "Combat/CombatBalance.h"
#include "LootBundle.h"
#include "StartechShipBase.h"
#include "StartechEnemyShip.generated.h"

class ALootPickup;
class UEnemyBrainComponent;
class UEnemyChargeComponent;
class UEnemyData;
class ULaserWeaponData;
class UMapMarkerComponent;
class UTargetableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyDied, AStartechEnemyShip*, Enemy, AActor*, Killer);

/** Native, for every enemy in every world: it died (hull 0), after its drop was rolled (GetLastDrop). The combat log listens. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnyEnemyDied, AStartechEnemyShip& /* Enemy */, AActor* /* Killer */);

// An AI ship. Its type (looks, stats, behaviour) comes from EnemyData; the brain flies it.
// Pooled by ProcSector: BeginPlay runs once per actor, so everything per-life is set in ResetForNewLife.
// Pooled enemies are spawned without a deferred step, so their EnemyData must be a class default:
// one Blueprint subclass per enemy type.
UCLASS()
class STARTECH_API AStartechEnemyShip : public AStartechShipBase, public IPoolable
{
	GENERATED_BODY()

public:
	AStartechEnemyShip();

	virtual void OnConstruction(const FTransform& Transform) override;

	const UEnemyData* GetEnemyData() const { return EnemyData; }

	const UEnemyBrainComponent* GetBrain() const { return Brain; }

	/** The charged shot (used only by types with FEnemyBehaviourBalance::bChargedShot). */
	UEnemyChargeComponent* GetChargedShot() const { return ChargedShot; }

	/** This life's tier damage multiplier (UStartechSettings::TierData's EnemyDamageMultiplier). */
	float GetLifeDamageMultiplier() const { return LifeDamageMultiplier; }

	/** The tier of the sector this life began in (see LifeTier). */
	int32 GetLifeTier() const { return LifeTier; }

	/** The laser fitted this life: DA_CombatBalance's weapon for the type, else EnemyData's. */
	const ULaserWeaponData* GetLaserWeapon() const { return LifeLaserWeapon; }

	/** The distance kept from the player while engaged, in cm: DA_CombatBalance's, else EnemyData's. */
	float GetPreferredRange() const { return LifePreferredRange; }

	/** This life's behaviour numbers: DA_CombatBalance's for the type, else defaults (everything off). */
	const FEnemyBehaviourBalance& GetBehaviour() const { return LifeBehaviour; }

	/** Gives up once the player is this far from home, in cm: DA_CombatBalance's HomeLeashRange, else EnemyData's. */
	float GetHomeLeashRange() const { return LifeHomeLeashRange; }

	/** Credits and loot amounts of the death drop × this: DA_CombatBalance's RewardMultiplier, else 1. */
	float GetRewardMultiplier() const { return LifeRewardMultiplier; }

	// For spawners: call on a deferred spawn, before FinishSpawning, so BeginPlay applies it.
	void SetEnemyData(UEnemyData* NewData);

	/** Pool hook: restores the enemy to fresh (ResetForNewLife), with its current location as home. */
	virtual void OnAcquiredFromPool_Implementation() override;

	/** Pool hook: stops the brain, weapons and flight. Safe on an enemy that is already dead. */
	virtual void OnReleasedToPool_Implementation() override;

	// At hull 0, after the drop was rolled. The wreck explodes, drops the loot and leaves play after the dying state.
	UPROPERTY(BlueprintAssignable, Category = "Enemy")
	FOnEnemyDied OnEnemyDied;

	/** See FOnAnyEnemyDied. */
	static FOnAnyEnemyDied OnAnyEnemyDied;

	/** What this life's death drops (rolled at hull 0, dropped when the wreck explodes; empty before, or with no loot table). */
	const FLootBundle& GetLastDrop() const { return LastDrop; }

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath(AActor* Killer) override;
	virtual void HandleDyingFinished(const FVector& Location) override;

	/** EnemyData's death effects, else the ship's. */
	virtual const FShipDeathEffects& GetDeathEffects() const override;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UTargetableComponent> Targetable;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UEnemyBrainComponent> Brain;

	// The ranged type's charged shot; idle for the others.
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UEnemyChargeComponent> ChargedShot;

	// Radar only. Disabled on death.
	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UMapMarkerComponent> MapMarker;

	// Per placed instance, so each one can be a different enemy type.
	UPROPERTY(EditAnywhere, Category = "Enemy")
	TObjectPtr<UEnemyData> EnemyData;

	// The pickup dropped on death, holding a roll of EnemyData's loot table. Set it in the Blueprint.
	UPROPERTY(EditDefaultsOnly, Category = "Loot")
	TSubclassOf<ALootPickup> LootPickupClass;

	// Seconds a dropped pickup lasts before it disappears. 0 keeps it until collected.
	UPROPERTY(EditDefaultsOnly, Category = "Loot", meta = (ClampMin = "0"))
	float LootLifespan = 120.f;

private:
	void ApplyMesh();
	void ApplyStats();

	// After the weapon system's BeginPlay, which replaces its stacks with the starting ammo.
	void ApplyAmmo();

	/** Rolls the death drop into LastDrop (the tier's credits, the reward multiplier, tier ammo drops). */
	void RollLoot();

	/** Drops LastDrop as a pickup at Location, once per life. */
	void SpawnLoot(const FVector& Location);

	/**
	 * Everything that must be fresh for each life: data-driven looks and stats scaled by the sector's tier, full health
	 * and not dead,
	 * no velocity or intents, weapons idle with no shots in flight, targetable and on radar, and the brain
	 * restarted with the current location as home. Runs from BeginPlay and OnAcquiredFromPool; idempotent.
	 */
	void ResetForNewLife();

	/** Stops firing and ends every laser shot in flight, so nothing from a previous life lands later. */
	void StopWeapons();

	/** LastDrop was rolled and not dropped yet. Cleared at the start of each life. */
	bool bLootPending = false;

	/**
	 * The tier of the sector this life began in (UStartechTierSubsystem's spawn tier), set by ApplyStats. Scales max
	 * hull, max shield and damage, and the credits of the death drop. Pooled enemies move between sectors, so it is
	 * read again for every life.
	 */
	int32 LifeTier = 1;

	/** See GetLaserWeapon. Set by ApplyStats. */
	UPROPERTY(Transient)
	TObjectPtr<ULaserWeaponData> LifeLaserWeapon;

	/** See GetPreferredRange. Set by ApplyStats. */
	float LifePreferredRange = 0.f;

	/** See GetRewardMultiplier. Set by ApplyStats. */
	float LifeRewardMultiplier = 1.f;

	/** See GetBehaviour. Set by ApplyStats. */
	UPROPERTY(Transient)
	FEnemyBehaviourBalance LifeBehaviour;

	/** See GetHomeLeashRange. Set by ApplyStats. */
	float LifeHomeLeashRange = 0.f;

	/** See GetLifeDamageMultiplier. Set by ApplyStats. */
	float LifeDamageMultiplier = 1.f;

	/** See GetLastDrop. Cleared by ApplyStats, set by RollLoot. */
	UPROPERTY(Transient)
	FLootBundle LastDrop;
};
