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
 * How one enemy type behaves (UEnemyBrainComponent): its speed, when it gives up, and whether it circles and
 * afterburns. A 0 switches a feature off or, where noted, falls back to the type's UEnemyData.
 */
USTRUCT(BlueprintType)
struct STARTECH_API FEnemyBehaviourBalance
{
	GENERATED_BODY()

	/** Forward top speed, cm/s. 0 uses UEnemyData::MaxForwardSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "cm/s"))
	float ForwardSpeed = 0.f;

	/**
	 * Forward top speed per tier (entry 0 = tier I), cm/s, for types whose speed must keep up with the player's Engines
	 * upgrade. A missing or 0 entry uses ForwardSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	TArray<float> ForwardSpeedByTier;

	/** This type's forward speed in Tier: ForwardSpeedByTier's entry, else ForwardSpeed (0: UEnemyData's). */
	float GetForwardSpeed(const int32 Tier) const
	{
		const int32 Index = Tier - 1;
		return ForwardSpeedByTier.IsValidIndex(Index) && ForwardSpeedByTier[Index] > 0.f ? ForwardSpeedByTier[Index] : ForwardSpeed;
	}

	/** Gives up once the player is this far from the enemy's home (its spawn point), cm. 0 uses UEnemyData::LeashRange. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "cm"))
	float HomeLeashRange = 0.f;

	/** Gives up after this long engaged without taking damage from the player (and, with bQuietCountsDealtDamage, without dealing any). 0 never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "s"))
	float LeashQuietSeconds = 0.f;

	/** Damage the enemy deals to the player also resets LeashQuietSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	bool bQuietCountsDealtDamage = false;

	/** Gives up after LeashFarSeconds with the player farther than this × its laser range. 0 never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0"))
	float LeashFarRangeFactor = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "s"))
	float LeashFarSeconds = 8.f;

	/**
	 * Circles the player: inside its laser range it strafes (swapping direction every few seconds) while holding its
	 * preferred range, and beyond it it approaches a point beside the player rather than the player itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	bool bCircle = false;

	/** Forward speed × this while afterburning. 1 or less: no afterburner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0"))
	float AfterburnMultiplier = 0.f;

	/** Longest burn; it also ends once the player is in laser range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "s"))
	float AfterburnSeconds = 3.f;

	/** Seconds after a burn before the next can start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", Units = "s"))
	float AfterburnCooldown = 6.f;

	/** A burn starts when the player is farther than this × its laser range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "1"))
	float AfterburnTriggerFactor = 1.2f;

	/**
	 * With the player inside this × its preferred range, it turns and flies away without firing (or charging) until it
	 * is back at its preferred range. Between this and the preferred range it reverses while firing, as every type
	 * does. 0 never retreats.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", ClampMax = "1"))
	float RetreatFraction = 0.f;

	/** Fires charged shots (UEnemyChargeComponent): a telegraph, then a slow projectile that can be dodged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot")
	bool bChargedShot = false;

	/** Seconds after engaging before the first charge, picked between these. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0", Units = "s"))
	float ChargeFirstDelayMin = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0", Units = "s"))
	float ChargeFirstDelayMax = 3.f;

	/** Seconds from one charge's start to the next, picked between these. It waits while it has no line of sight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0", Units = "s"))
	float ChargeIntervalMin = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0", Units = "s"))
	float ChargeIntervalMax = 8.f;

	/** The telegraph: glow, charge effect and HUD warning, then the shot. Normal fire pauses meanwhile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0", Units = "s"))
	float ChargeTelegraphSeconds = 1.2f;

	/** Damage of one charged shot, before the tier's EnemyDamageMultiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0"))
	float ChargedShotDamage = 40.f;

	/** Projectile speed. At the preferred range it should take about 0.6–0.8 s to arrive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "1", Units = "cm/s"))
	float ChargedShotSpeed = 12500.f;

	/** Radius of the projectile's sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0", Units = "cm"))
	float ChargedShotRadius = 150.f;

	/** Seconds the projectile flies before it is gone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charged shot", meta = (ClampMin = "0.1", Units = "s"))
	float ChargedShotLifetime = 2.f;
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

	/** How it behaves: speed, leash, circling, afterburner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	FEnemyBehaviourBalance Behaviour;

	/**
	 * TTK table only: the share of a fight it spends firing its lasers (turning, closing in, retreating and charging
	 * don't fire), measured from play sessions (Startech.Combat.Report's Uptime column). 1 = the old worst case.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table", meta = (ClampMin = "0", ClampMax = "1"))
	float FireUptime = 1.f;

	/**
	 * TTK table only: the same in the type's second case: against a player who backs off (afterburner types). Charged-shot
	 * types use FireUptime for both of theirs (every charged shot hit, and every one dodged).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table", meta = (ClampMin = "0", ClampMax = "1"))
	float FireUptimeAlt = 1.f;

	/** TTK table: this type's own one-fight health-loss target (0 max: the role's target from OneFightLossMin/Max). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table", meta = (ClampMin = "0", ClampMax = "1"))
	float LossTargetMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table", meta = (ClampMin = "0", ClampMax = "1"))
	float LossTargetMax = 0.f;

	/** TTK table: the target of the second case (all charged shots dodged, or the player backing off). 0 max: none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table", meta = (ClampMin = "0", ClampMax = "1"))
	float LossAltTargetMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table", meta = (ClampMin = "0", ClampMax = "1"))
	float LossAltTargetMax = 0.f;

	/**
	 * The second case's target is shown for information only: never marked outside and never failing
	 * Startech.Combat.TTKTargets (the chaser's back-off value, which varies 3-5 points between play sessions).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table")
	bool bLossAltTargetInfoOnly = false;

	/** Only the Regular role's rows (the tiers where the type is the norm) are held to the type's own loss targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TTK table")
	bool bLossTargetsRegularOnly = false;
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
