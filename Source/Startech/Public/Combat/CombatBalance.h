#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatBalance.generated.h"

class AStartechShipPawn;
class UAmmoData;
class UEnemyData;
class UItemData;
class ULaserWeaponData;
class UShopData;
class UTradeableData;
struct FLootBundle;

/** One ammo's damage and price. */
USTRUCT(BlueprintType)
struct STARTECH_API FAmmoBalance
{
	GENERATED_BODY()

	/** The ammo (Item.Ammo.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (Categories = "Item.Ammo"))
	FGameplayTag AmmoTag;

	/** Shot damage × this. Replaces UAmmoData::DamageMultiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.f;

	/** Credits per round before the shop's markup. Replaces UTradeableData::BaseValue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0"))
	float BaseValue = 1.f;
};

/** What an enemy type is in one tier, for the time-to-kill and damage targets. */
UENUM(BlueprintType)
enum class ECombatRole : uint8
{
	/** Held to the targets. */
	Regular,
	/** Allowed up to ToughFactor × the targets. */
	Tough,
	/** Weaker than the tier's regulars: no target. */
	Weak
};

/** An enemy type's role in one tier. */
USTRUCT(BlueprintType)
struct STARTECH_API FEnemyTierRole
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "1"))
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	ECombatRole Role = ECombatRole::Regular;
};

/**
 * One enemy type's combat numbers, before the tier multipliers (UStartechSettings::TierData). Replaces UEnemyData's
 * MaxHull, MaxShield, LaserWeapon and PreferredRange and the weapon's damage; applied by AStartechEnemyShip::ApplyStats.
 */
USTRUCT(BlueprintType)
struct STARTECH_API FEnemyBalance
{
	GENERATED_BODY()

	/** The type (Enemy.*), matched against UEnemyData::EnemyTags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (Categories = "Enemy"))
	FGameplayTag EnemyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0"))
	float Hull = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0"))
	float Shield = 100.f;

	/** Fitted to both laser mounts: its fire rate and range apply, its damage is replaced by DamagePerShot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<ULaserWeaponData> LaserWeapon = nullptr;

	/** Damage of one shot of one mount, before the tier's EnemyDamageMultiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0"))
	float DamagePerShot = 1.f;

	/**
	 * Distance kept from the player while engaged, in cm. The brain holds anywhere within its RangeTolerance of it, so
	 * keep PreferredRange × (1 + RangeTolerance) inside the weapon's range or the enemy never fires.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0"))
	float PreferredRange = 3000.f;

	/** The kill's credits and loot amounts × this (tougher types pay more). Loot tables still pick what drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0"))
	float RewardMultiplier = 1.f;

	/** The tiers it appears in and its role there (the TTK table's rows). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	TArray<FEnemyTierRole> Appearances;
};

/** The gear a player should have when fighting in a tier, and the time to kill a regular enemy with it. */
USTRUCT(BlueprintType)
struct STARTECH_API FCombatGearCase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "1"))
	int32 Tier = 1;

	/** Laser ammo fired. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (Categories = "Item.Ammo"))
	FGameplayTag AmmoTag;

	/** Upgrade levels (UpgradeCatalog's LasersTrackId, HullTrackId and ShieldTrackId). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0"))
	int32 LasersLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0"))
	int32 HullLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0"))
	int32 ShieldLevel = 0;

	/** Time to kill a regular enemy, in seconds on target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0", Units = "s"))
	float MinTimeToKill = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0", Units = "s"))
	float MaxTimeToKill = 5.f;

	/** Accepted miss either side of the time-to-kill target (tier IV chasers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0", Units = "s"))
	float ToleranceSeconds = 0.f;

	/** False for bonus gear (LL4) that only the time-to-kill target applies to, not the damage target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	bool bCheckDamage = true;
};

/** Ammo the shop sells only once the player has reached a sector tier. */
USTRUCT(BlueprintType)
struct STARTECH_API FAmmoUnlock
{
	GENERATED_BODY()

	/** The locked ammo (Item.Ammo.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (Categories = "Item.Ammo"))
	FGameplayTag AmmoTag;

	/** Sold once UPlayerProfileSubsystem::GetHighestTierVisited() reaches this tier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "1"))
	int32 RequiredTier = 1;

	/** The disabled Buy button's label until then: "Reach Glacier (tier III)". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	FText LockedReason;
};

/** Ammo that a destroyed enemy of one sector tier can drop, rolled on top of its loot table. */
USTRUCT(BlueprintType)
struct STARTECH_API FTierAmmoDrop
{
	GENERATED_BODY()

	/** The enemy's tier (its life tier, the tier it spawned in). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 Tier = 1;

	/** The ammo dropped (Item.Ammo.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (Categories = "Item.Ammo"))
	FGameplayTag AmmoTag;

	/** Chance per kill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 0.f;

	/** Rounds dropped, not scaled by the tier's loot multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

/**
 * Every combat balance number in one place (tier multipliers stay in UStartechSettings::TierData): DA_CombatBalance, chosen in UStartechSettings::CombatBalance and made by
 * Tools/Combat/setup_combat_balance.py.
 *
 * Ammo progression: FreeAmmoTag (LL1) is free and unlimited, never used up, stored, sold or dropped. AmmoUnlocks keep
 * ammo out of the shop until a tier is reached (LL3 at tier III). DropOnlyAmmoTags (LL4) are never sold, only dropped
 * through TierAmmoDrops.
 */
UCLASS(BlueprintType)
class STARTECH_API UCombatBalance : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The asset in UStartechSettings::CombatBalance, loaded; null (logged once) when it isn't set. */
	static const UCombatBalance* Get();

	/** True when Good is the free ammo. */
	bool IsFreeAmmo(const UTradeableData* Good) const;

	/** True when Good is ammo that is never sold. */
	bool IsDropOnly(const UTradeableData* Good) const;

	/** The free ammo, resolved through UStartechSettings::ItemCatalog; null when unset or not in the catalog. */
	UAmmoData* FindFreeAmmo() const;

	/**
	 * True when the shop must not sell AmmoTag yet: HighestTier (the highest tier the player has reached) is below its
	 * unlock's RequiredTier. OutReason is then that unlock's LockedReason.
	 */
	bool IsAmmoLocked(const FGameplayTag& AmmoTag, int32 HighestTier, FText& OutReason) const;

	/** Rolls every TierAmmoDrops entry of Tier and adds what drops to Bundle. */
	void RollTierAmmoDrops(int32 Tier, FLootBundle& Bundle) const;

	/** Ammo's entry in Ammo, or null. */
	const FAmmoBalance* FindAmmo(const UAmmoData* Ammo) const;

	/** The first entry of Enemies whose EnemyTag is one of Data's EnemyTags, or null. */
	const FEnemyBalance* FindEnemy(const UEnemyData* Data) const;

	/** Ammo's damage multiplier: its Ammo entry, else the asset's own. 1 for null. */
	static float GetDamageMultiplier(const UAmmoData* Ammo);

	/** Good's base value: its Ammo entry when it is listed ammo, else the asset's own. */
	static double GetBaseValue(const UTradeableData& Good);

	/** What firing one round of Ammo costs: AmmoShop's price, or 0 for free, drop-only or unsold ammo. */
	double GetRoundCost(const UAmmoData* Ammo) const;

	/** What one unit of Item sells for at LootShop, or 0 if LootShop doesn't buy it. */
	double GetSellValue(const UItemData* Item) const;

	/** Bundle's credits plus its items' sell value (ammo can't be sold: 0). */
	double GetBundleValue(const FLootBundle& Bundle) const;

	/** LL1: free and unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (Categories = "Item.Ammo"))
	FGameplayTag FreeAmmoTag;

	/** Ammo never sold (LL4); only TierAmmoDrops hand it out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (Categories = "Item.Ammo"))
	TArray<FGameplayTag> DropOnlyAmmoTags;

	/** Ammo locked in the shop until a tier is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TArray<FAmmoUnlock> AmmoUnlocks;

	/** Extra ammo drops by enemy tier (LL4 in tiers III and IV). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FTierAmmoDrop> TierAmmoDrops;

	/**
	 * Radius, in cm, of the sphere every laser and drainer shot sweeps (UWeaponMountComponent::TraceShot). 0 is a plain
	 * line. A line aimed at a ship's centre can pass between the convex pieces of its collision and never hit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons", meta = (ClampMin = "0", Units = "cm"))
	float ShotRadius = 50.f;

	/** Laser ammo damage and prices (LL1-LL4). Ammo not listed uses its asset's values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TArray<FAmmoBalance> Ammo;

	/** Every enemy type. A type not listed uses its UEnemyData values (logged once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemies")
	TArray<FEnemyBalance> Enemies;

	/** Expected gear and time-to-kill target per tier (Startech.Combat.TTKTable's rows). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	TArray<FCombatGearCase> GearCases;

	/** Share of the expected health one fight with a regular enemy may cost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "0", ClampMax = "1"))
	float OneFightLossMin = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "0", ClampMax = "1"))
	float OneFightLossMax = 0.25f;

	/** Tough types may take up to this × the time-to-kill and damage targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "1"))
	float ToughFactor = 2.f;

	/** The money rule: ammo spent on a kill at most this share of what the kill pays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxAmmoCostShare = 0.3f;

	/** Upgrade tracks of the expected gear's levels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	FName LasersTrackId = TEXT("Lasers");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	FName HullTrackId = TEXT("Hull");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	FName ShieldTrackId = TEXT("ShieldCapacity");

	/** The player ship, for its base hull and shield. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	TSoftClassPtr<AStartechShipPawn> PlayerShipClass;

	/** The shop whose prices value ammo spent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	TSoftObjectPtr<UShopData> AmmoShop;

	/** The shop whose buy prices value the loot of a kill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targets")
	TSoftObjectPtr<UShopData> LootShop;

private:
	/** True when Good is ammo whose ItemTag is in Tags. */
	static bool HasAmmoTag(const UTradeableData* Good, const TArray<FGameplayTag>& Tags);
};
