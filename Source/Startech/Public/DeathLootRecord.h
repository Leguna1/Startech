#pragma once

#include "CoreMinimal.h"
#include "LootBundle.h"
#include "DeathLootRecord.generated.h"

class ALootPickup;

/**
 * Cargo the player dropped on death, tracked by UDespawnSubsystem so it survives warping away and back.
 * While the pickup is in play, Pickup points at it. While the player is in another sector, Pickup is null and
 * Contents and Location hold what the pickup had when the sector was left.
 */
USTRUCT()
struct STARTECH_API FDeathLootRecord
{
	GENERATED_BODY()

	/** The pickup in play, or null while the record is stored away from its sector. */
	UPROPERTY()
	TWeakObjectPtr<ALootPickup> Pickup;

	/** Pickup class to acquire from the pool when the record is restored. */
	UPROPERTY()
	TSubclassOf<ALootPickup> PickupClass;

	/** What was left in the pickup when its sector was left. Only meaningful while stored. */
	UPROPERTY()
	FLootBundle Contents;

	/** Where the pickup was when its sector was left. Only meaningful while stored. */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Sector the death happened in; the record is restored only when that sector is ready again. */
	UPROPERTY()
	FName SectorId;

	/** World time (UWorld::GetTimeSeconds) at which the cargo is gone for good. Negative never expires. */
	UPROPERTY()
	double ExpiryTime = -1.0;

	/** True once ExpiryTime has passed at world time Now. */
	bool IsExpired(const double Now) const { return ExpiryTime >= 0.0 && Now >= ExpiryTime; }
};
