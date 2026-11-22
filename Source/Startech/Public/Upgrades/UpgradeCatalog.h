#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UpgradeCatalog.generated.h"

/** How an upgrade's bonus combines with the ship's base value. */
UENUM(BlueprintType)
enum class EUpgradeBonusKind : uint8
{
	/** A fraction of the base: 0.2 is +20%. Summed per stat into UShipUpgradeSubsystem::GetStatMultiplier. */
	Percent,

	/** Units added to the base: +100 hull, +20 cargo. Summed per stat into UShipUpgradeSubsystem::GetStatBonus. */
	Flat,
};

/** One stat a track changes, and by how much at each level. */
USTRUCT(BlueprintType)
struct STARTECH_API FUpgradeStatEffect
{
	GENERATED_BODY()

	/** The stat (Stat.*), read by the ship's systems. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Stat"))
	FGameplayTag StatTag;

	/** The short stat name in the hub: "Hull" in "Hull 300 → 400". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText StatLabel;

	/** Percent of the base, or flat units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	EUpgradeBonusKind Kind = EUpgradeBonusKind::Percent;

	/**
	 * The total bonus at each level, not the step: entry 0 is level 1. A fraction for Percent (0.4 = +40% at that
	 * level), units for Flat. Levels past the end use the last entry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<float> BonusAtLevel;

	/** The total bonus at Level (0 = none). */
	float GetBonus(int32 Level) const;
};

/** A count of one mineral, by item tag. */
USTRUCT(BlueprintType)
struct STARTECH_API FUpgradeMineralCost
{
	GENERATED_BODY()

	/** The mineral's ItemTag (Item.Mineral.*), resolved through UStartechSettings::ItemCatalog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Item.Mineral"))
	FGameplayTag MineralTag;

	/** How many. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 Count = 0;
};

/** What buying one level costs. */
USTRUCT(BlueprintType)
struct STARTECH_API FUpgradeLevelCost
{
	GENERATED_BODY()

	/** Credits, before UStartechSettings::UpgradeCostScale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 Credits = 0;

	/** Minerals taken from the cargo hold. Never scaled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeMineralCost> Minerals;
};

/** One upgrade track: a ship system levelled from 0 to its number of costs. */
USTRUCT(BlueprintType)
struct STARTECH_API FUpgradeTrack
{
	GENERATED_BODY()

	/** The id saved in Startech.Upgrades and used by the test commands. Never rename one in use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FName TrackId;

	/** "Hull plating". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText DisplayName;

	/** One line for the hub. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FText Description;

	/** The stats it changes; the Booster has two. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeStatEffect> Effects;

	/** One entry per level: entry 0 buys level 1. The number of entries is the track's max level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FUpgradeLevelCost> LevelCosts;

	/** The highest level: the number of LevelCosts. */
	int32 GetMaxLevel() const { return LevelCosts.Num(); }
};

/**
 * DA_UpgradeCatalog: every upgrade track (UStartechSettings::UpgradeCatalog). Made by
 * Tools/Upgrades/create_upgrade_catalog.py. Read it through UShipUpgradeSubsystem.
 */
UCLASS(BlueprintType)
class STARTECH_API UUpgradeCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The tracks, in the hub's order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrades")
	TArray<FUpgradeTrack> Tracks;

	/** The track with TrackId, or null. */
	const FUpgradeTrack* FindTrack(FName TrackId) const;
};
