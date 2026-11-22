#include "StartechGameplayTags.h"

namespace StartechTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy, "Enemy", "Any enemy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Drifter, "Enemy.Drifter", "Passive drifter enemy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Chaser, "Enemy.Chaser", "Chaser enemy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Ranged, "Enemy.Ranged", "Ranged enemy");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item, "Item", "Any item");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Mineral, "Item.Mineral", "Any mineral (cargo)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Mineral_Ferrite, "Item.Mineral.Ferrite", "Ferrite");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Mineral_Cobaltine, "Item.Mineral.Cobaltine", "Cobaltine");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Mineral_Aurium, "Item.Mineral.Aurium", "Aurium");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo, "Item.Ammo", "Any ammo");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_LL1, "Item.Ammo.LL1", "Laser ammo LL1 (basic)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_LL2, "Item.Ammo.LL2", "Laser ammo LL2");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_LL3, "Item.Ammo.LL3", "Laser ammo LL3");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_LL4, "Item.Ammo.LL4", "Laser ammo LL4");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_SD1, "Item.Ammo.SD1", "Shield drain ammo SD1");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo_SD2, "Item.Ammo.SD2", "Shield drain ammo SD2");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Weapon, "Item.Weapon", "Any weapon (fitted to a mount or spare)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Weapon_Laser, "Item.Weapon.Laser", "The player's basic laser (DA_BasicWeapon)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Weapon_ShieldDrainer, "Item.Weapon.ShieldDrainer", "The shield drainer (DA_ShieldDrainer)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Reward_Credits, "Reward.Credits", "Quest reward: credits");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat, "Stat", "Any ship stat an upgrade can change");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Hull_Max, "Stat.Hull.Max", "Max hull");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Shield_Max, "Stat.Shield.Max", "Max shield");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Shield_Regen, "Stat.Shield.Regen", "Shield regeneration per second");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Cargo_Capacity, "Stat.Cargo.Capacity", "Cargo capacity");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Engine_Speed, "Stat.Engine.Speed", "Top speeds and acceleration");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Laser_Damage, "Stat.Laser.Damage", "Laser damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Drainer_Power, "Stat.Drainer.Power", "Shield drainer power");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Boost_Energy, "Stat.Boost.Energy", "Max boost energy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Stat_Boost_Regen, "Stat.Boost.Regen", "Boost energy regeneration");
}
