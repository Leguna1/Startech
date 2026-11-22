#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Startech's native Gameplay Tags, all defined in StartechGameplayTags.cpp. Declared natively rather than in
 * DefaultGameplayTags.ini so C++ refers to them by compile-checked names and they exist as soon as the module loads
 * (before any data asset or editor script reads them).
 *
 * Quest conventions: enemies carry Enemy.* tags on UEnemyData::EnemyTags, cargo and ammo carry Item.* tags on
 * UTradeableData::ItemTag. A quest reward of an item or ammo uses the item tag itself; Reward.Credits grants credits.
 */
namespace StartechTags
{
	/** Parent of every enemy type; a quest target of Enemy matches all of them. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy);
	/** The passive drifter. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Drifter);
	/** The chaser, which closes in. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Chaser);
	/** The ranged enemy, which keeps its distance. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Ranged);

	/** Parent of every item tag. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item);
	/** Parent of every mineral (cargo). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Mineral);
	/** Mineral: Ferrite. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Mineral_Ferrite);
	/** Mineral: Cobaltine. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Mineral_Cobaltine);
	/** Mineral: Aurium. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Mineral_Aurium);

	/** Parent of every ammo type (held by the weapon system, not the cargo hold). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo);
	/** Laser ammo LL1, the basic type. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_LL1);
	/** Laser ammo LL2. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_LL2);
	/** Laser ammo LL3. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_LL3);
	/** Laser ammo LL4. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_LL4);
	/** Shield drain ammo SD1. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_SD1);
	/** Shield drain ammo SD2. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo_SD2);

	/** Quest reward: Amount credits. */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Weapon);
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Weapon_Laser);
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Weapon_ShieldDrainer);
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reward_Credits);

	/** Parent of every ship stat an upgrade track can change (FUpgradeStatEffect::StatTag). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat);
	/** Max hull (UHealthComponent). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Hull_Max);
	/** Max shield (UHealthComponent). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Shield_Max);
	/** Shield regeneration per second (UHealthComponent). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Shield_Regen);
	/** Cargo capacity in units (UInventoryComponent). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Cargo_Capacity);
	/** Every top speed and the acceleration (AStartechShipBase::SetFlightScales, normal scale). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Engine_Speed);
	/** Laser damage (the laser group's mounts). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Laser_Damage);
	/** Shield drainer power (the shield drain group's mounts). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Drainer_Power);
	/** Max boost energy (UShipBoostComponent). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Boost_Energy);
	/** Boost energy regeneration (UShipBoostComponent). */
	STARTECH_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Boost_Regen);
}
