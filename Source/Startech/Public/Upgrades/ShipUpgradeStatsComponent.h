#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ShipUpgradeStatsComponent.generated.h"

class AStartechShipPawn;

/**
 * Applies UShipUpgradeSubsystem's stats to the player ship it sits on (subobject UpgradeStats of AStartechShipPawn;
 * enemies don't have one). Remembers each system's base value at BeginPlay (BP_ShipPawn's values), then sets
 * value = (base + GetStatBonus) × GetStatMultiplier on:
 *   max hull and max shield (raising one raises the current value by the same amount), shield regeneration,
 *   cargo capacity, the flight model's normal speed scale, every laser and shield drain mount's damage scale, and the
 *   boost's energy and regeneration.
 * Scale stats (speed, damage, boost) use a base of 1. Reapplied on OnUpgradeChanged, at BeginPlay (after a load) and
 * on respawn (AStartechShipPawn::RespawnAt). A change while docked (a purchase) also fills hull and shield to the new
 * maximums, since docking is a full repair.
 */
UCLASS(ClassGroup = (Startech))
class STARTECH_API UShipUpgradeStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShipUpgradeStatsComponent();

	/** Sets every upgraded stat from the current levels. */
	void ApplyStats();

	/** BP_ShipPawn's max hull, before upgrades. */
	float GetBaseMaxHull() const { return BaseMaxHull; }

	/** BP_ShipPawn's max shield, before upgrades. */
	float GetBaseMaxShield() const { return BaseMaxShield; }

	/** BP_ShipPawn's shield regeneration per second, before upgrades. */
	float GetBaseShieldRegen() const { return BaseShieldRegen; }

	/** BP_ShipPawn's cargo capacity, before upgrades. */
	int32 GetBaseCargoCapacity() const { return BaseCargoCapacity; }

	/** (Base + bonus) × multiplier for StatTag at the current levels, or Base without the subsystem. */
	float GetUpgradedValue(const FGameplayTag& StatTag, float Base) const;

protected:
	/** Remembers the base values, binds the subsystem and applies the stats. */
	virtual void BeginPlay() override;

	/** Unbinds the subsystem. */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	/** Subsystem callback: any level changed. */
	UFUNCTION()
	void HandleUpgradeChanged(FName TrackId, int32 NewLevel);

	/** The ship this sits on. */
	AStartechShipPawn* GetShip() const;

	float BaseMaxHull = 0.f;
	float BaseMaxShield = 0.f;
	float BaseShieldRegen = 0.f;
	int32 BaseCargoCapacity = 0;

	/** True once the base values are remembered. */
	bool bHasBase = false;
};
