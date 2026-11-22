#pragma once

#include "CoreMinimal.h"

class AStartechShipPawn;
class UShipUpgradeSubsystem;
struct FUpgradeStatEffect;

/**
 * Text for the hub's Upgrades tab: what a stat is now and what it becomes at a track's next level, computed like
 * UShipUpgradeStatsComponent applies it, (base + bonus) × multiplier, so other tracks on the same stat are included.
 * Units per stat: hull, shield, cargo and boost energy as whole numbers; shield and boost recharge per second; engines
 * as top speed in m/s; laser damage and drainer power as a percentage of the base (100%).
 */
struct STARTECH_API FUpgradeStatFormat
{
	/** "Hull 300 → 400" for Effect when TrackId goes from FromLevel to ToLevel. */
	static FText FormatChange(const FUpgradeStatEffect& Effect, FName TrackId, int32 FromLevel, int32 ToLevel,
		const AStartechShipPawn& Ship, const UShipUpgradeSubsystem& Upgrades);

	/** "Hull 600" for Effect at TrackId's Level (the max-level row). */
	static FText FormatValue(const FUpgradeStatEffect& Effect, FName TrackId, int32 Level, const AStartechShipPawn& Ship,
		const UShipUpgradeSubsystem& Upgrades);

private:
	/** The stat's value with TrackId at Level, in display units. */
	static float GetDisplayValue(const FUpgradeStatEffect& Effect, FName TrackId, int32 Level,
		const AStartechShipPawn& Ship, const UShipUpgradeSubsystem& Upgrades);

	/** Value as text with the stat's unit: "400", "11.5/s", "31.8 m/s", "112%". */
	static FText FormatNumber(const FUpgradeStatEffect& Effect, float Value);
};
