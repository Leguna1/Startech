#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Save/StartechSaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Upgrades/UpgradeCatalog.h"
#include "ShipUpgradeSubsystem.generated.h"

class AStartechShipPawn;

/**
 * Broadcast when an upgrade level changes. TrackId is the track bought or set; NAME_None means every track may have
 * changed (a load or a new game), so listeners re-read everything.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUpgradeChanged, FName, TrackId, int32, NewLevel);

/**
 * The player's upgrade levels, one per track of DA_UpgradeCatalog (UStartechSettings::UpgradeCatalog), for the whole
 * game session. Saved as Startech.Upgrades. Buying takes credits and minerals from the player ship and needs it docked.
 * The ship applies the stats through UShipUpgradeStatsComponent; enemies never read them.
 */
UCLASS()
class STARTECH_API UShipUpgradeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of WorldContext's game instance, or null. */
	static UShipUpgradeSubsystem* Get(const UObject* WorldContext);

	/** The catalog, loaded on first use; null (with an error logged once) when the setting is empty. */
	const UUpgradeCatalog* GetCatalog() const;

	/** The track with TrackId, or null. */
	const FUpgradeTrack* FindTrack(FName TrackId) const;

	/** The level of TrackId: 0 to its max level. */
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetLevel(FName TrackId) const;

	/** The highest level of TrackId; 0 for an unknown track. */
	UFUNCTION(BlueprintPure, Category = "Upgrades")
	int32 GetMaxLevel(FName TrackId) const;

	/**
	 * What buying Level of TrackId costs, with the credits scaled by UStartechSettings::UpgradeCostScale (rounded to
	 * the nearest 10). False when the track or the level doesn't exist.
	 */
	bool GetLevelCost(FName TrackId, int32 Level, FUpgradeLevelCost& OutCost) const;

	/**
	 * True when the next level of TrackId can be bought now. Otherwise OutReason says why, in this order:
	 * "Max level", "Dock to upgrade", "Need 1,500 more credits", "Need 12 more Ferrite".
	 */
	bool CanUpgrade(FName TrackId, FText& OutReason) const;

	/**
	 * Buys the next level of TrackId: takes the credits and minerals from the player ship, raises the level and fires
	 * OnUpgradeChanged. False (nothing taken) when CanUpgrade refuses.
	 */
	bool Upgrade(FName TrackId);

	/**
	 * Sets TrackId to Level (clamped to 0 to the max level) without any cost, and fires OnUpgradeChanged. For test
	 * commands. False for an unknown track.
	 */
	bool SetLevel(FName TrackId, int32 Level);

	/**
	 * 1 plus every Percent bonus for StatTag at the current levels: 1.4 for +40%. A system's value is
	 * (base + GetStatBonus) × GetStatMultiplier.
	 */
	float GetStatMultiplier(const FGameplayTag& StatTag) const;

	/** The sum of every Flat bonus for StatTag at the current levels: +300 hull. */
	float GetStatBonus(const FGameplayTag& StatTag) const;

	/** GetStatMultiplier as if TrackId were at Level: the hub's "next level" preview. */
	float GetStatMultiplierWith(const FGameplayTag& StatTag, FName TrackId, int32 Level) const;

	/** GetStatBonus as if TrackId were at Level. */
	float GetStatBonusWith(const FGameplayTag& StatTag, FName TrackId, int32 Level) const;

	/** Every track above level 0, for the save. */
	const TArray<FStartechUpgradeLevel>& GetLevels() const { return Levels; }

	/**
	 * Loading a save: replaces every level (unknown tracks are dropped with a warning, levels clamped) and fires
	 * OnUpgradeChanged(NAME_None).
	 */
	void RestoreLevels(const TArray<FStartechUpgradeLevel>& NewLevels);

	/** A new game: every track to 0, and OnUpgradeChanged(NAME_None). */
	void ResetLevels();

	/** Fires on every level change: a purchase, a test command, a load or a new game. */
	UPROPERTY(BlueprintAssignable, Category = "Upgrades")
	FOnUpgradeChanged OnUpgradeChanged;

private:
	/** The player's ship, or null (menu, dead world). */
	AStartechShipPawn* GetPlayerShip() const;

	/** Sums the Kind bonuses for StatTag, with OverrideTrack at OverrideLevel when it is set. */
	float SumBonuses(const FGameplayTag& StatTag, EUpgradeBonusKind Kind, FName OverrideTrack, int32 OverrideLevel) const;

	/** Stores Level for TrackId (removing the entry at 0). */
	void StoreLevel(FName TrackId, int32 Level);

	/** The levels above 0. */
	TArray<FStartechUpgradeLevel> Levels;

	/** The loaded catalog. */
	UPROPERTY(Transient)
	mutable TObjectPtr<const UUpgradeCatalog> Catalog;

	/** True once a missing catalog has been reported. */
	mutable bool bReportedMissingCatalog = false;
};
