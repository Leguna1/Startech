#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBalance.h"

/**
 * One enemy type in one tier, fought with one expected-gear case of DA_CombatBalance. Both sides hit every shot and the
 * enemy fires for the whole fight (the worst case); shield regeneration never starts under sustained fire.
 */
struct STARTECH_API FCombatTTKRow
{
	int32 Tier = 1;

	/** The enemy type's tag leaf ("Chaser"). */
	FString Enemy;

	ECombatRole Role = ECombatRole::Regular;

	/** "LL3 L5". */
	FString Gear;

	/** Seconds on target. */
	float TimeToKill = 0.f;

	/** Share of the expected health lost fighting one, and three at once (killed one after another: 3 + 2 + 1). */
	float LoseOne = 0.f;
	float LoseThree = 0.f;

	/** Rounds fired (all mounts) and their cost at the ammo shop; free and drop-only ammo cost nothing. */
	int32 Rounds = 0;
	double AmmoCost = 0.0;

	/** Average credits plus the sell value of the average loot (UCombatBalance::LootShop's buy prices). */
	double KillValue = 0.0;

	/** AmmoCost / KillValue. */
	double CostShare = 0.0;

	/** The allowed time to kill for this row's role (0 max: no target). */
	float AllowedMinTimeToKill = 0.f;
	float AllowedMaxTimeToKill = 0.f;

	/** The allowed one-fight loss for this row's role (0 max: no target). */
	float AllowedMinLoss = 0.f;
	float AllowedMaxLoss = 0.f;

	bool bTimeToKillOutside = false;
	bool bLossOutside = false;
	bool bCostOutside = false;

	bool IsOutside() const { return bTimeToKillOutside || bLossOutside || bCostOutside; }
};

/**
 * The Phase 0 table computed from the live data: DA_CombatBalance (enemies, ammo, gear cases, targets), the tier
 * multipliers (UStartechSettings::TierData), the player's lasers (NewGameMountWeapons), the upgrade catalog, the player
 * ship's base health and the enemies' loot tables. Startech.Combat.TTKTable prints it; Startech.Combat.TTKTargets
 * tests it. Needs no world.
 */
struct STARTECH_API FCombatTTKTable
{
	TArray<FCombatTTKRow> Rows;

	/** Problems that are not one row's value: missing data, a preferred range beyond the gun's reach. */
	TArray<FString> Errors;

	/** Notes: asset values that differ from DA_CombatBalance (the balance wins at runtime). */
	TArray<FString> Notes;

	/** Computes the table from Balance. */
	static FCombatTTKTable Build(const UCombatBalance& Balance);

	/** The table as text lines, values outside their targets marked with '*'. */
	TArray<FString> ToLines() const;

	/** Rows with a value outside its target. */
	int32 CountOutside() const;
};
