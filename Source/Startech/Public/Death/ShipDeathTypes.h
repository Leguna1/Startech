#pragma once

#include "CoreMinimal.h"
#include "ShipDeathTypes.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * The effect hooks of one ship type's death (UShipDeathComponent). Every field is optional: an empty one falls back to
 * the same field of UStartechSettings::Death.DefaultEffects, and an empty default plays nothing.
 */
USTRUCT(BlueprintType)
struct STARTECH_API FShipDeathEffects
{
	GENERATED_BODY()

	/** Spawned at the wreck's location when the dying state ends, just before the ship hides and drops its loot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TSoftObjectPtr<UNiagaraSystem> DeathExplosionFX;

	/** Scale of DeathExplosionFX. 0 uses the default's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0"))
	float DeathExplosionScale = 0.f;

	/** Spawned at random points on the hull during the dying state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TSoftObjectPtr<UNiagaraSystem> SecondaryExplosionFX;

	/** Scale of SecondaryExplosionFX. 0 uses the default's. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death", meta = (ClampMin = "0"))
	float SecondaryExplosionScale = 0.f;

	/** Played at the wreck's location with DeathExplosionFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	TSoftObjectPtr<USoundBase> DeathSound;

	/** Override's fields where set, Default's elsewhere. Scales end up at least 0.01. */
	static FShipDeathEffects Resolve(const FShipDeathEffects& Override, const FShipDeathEffects& Default);
};

/** How every ship dies: the length of the dying state, its motion and flash, and the default effect hooks. */
USTRUCT(BlueprintType)
struct STARTECH_API FShipDeathSettings
{
	GENERATED_BODY()

	/** Seconds between hull 0 and the final explosion, for enemies and the player. 0 explodes at once. */
	UPROPERTY(EditAnywhere, Category = "Death", meta = (ClampMin = "0", Units = "s"))
	float DeathDuration = 1.f;

	/** Effects for any ship type that doesn't set its own (UEnemyData::DeathEffects, AStartechShipBase::DeathEffects). */
	UPROPERTY(EditAnywhere, Category = "Death")
	FShipDeathEffects DefaultEffects;

	/** Number of secondary explosions, picked between these, spread over the middle of the dying state. */
	UPROPERTY(EditAnywhere, Category = "Death", meta = (ClampMin = "0"))
	int32 MinSecondaryExplosions = 2;

	UPROPERTY(EditAnywhere, Category = "Death", meta = (ClampMin = "0"))
	int32 MaxSecondaryExplosions = 3;

	/** Tumble rate reached at the end of the dying state, about a random axis. It starts at a tenth of this. */
	UPROPERTY(EditAnywhere, Category = "Death", meta = (ClampMin = "0", Units = "deg/s"))
	float MaxTumbleRate = 120.f;

	/** How far the hull colours blend towards white at the end (0 none, 1 fully white). */
	UPROPERTY(EditAnywhere, Category = "Death|Flash", meta = (ClampMin = "0", ClampMax = "1"))
	float FlashMaxWhite = 0.85f;

	/** The emission parameter is multiplied by 1 + this × the flash. */
	UPROPERTY(EditAnywhere, Category = "Death|Flash", meta = (ClampMin = "0"))
	float FlashEmissionBoost = 6.f;

	/** Flicker frequency of the flash. */
	UPROPERTY(EditAnywhere, Category = "Death|Flash", meta = (ClampMin = "0", Units = "Hz"))
	float FlashFlickerHz = 12.f;

	/** Vector parameters blended towards white. Missing ones are skipped (M_Colorize has all three). */
	UPROPERTY(EditAnywhere, Category = "Death|Flash")
	TArray<FName> FlashColorParameters = { TEXT("Color 1"), TEXT("Color 2"), TEXT("Color 3") };

	/** Scalar parameter raised by the flash. None, or missing from the material, skips it. */
	UPROPERTY(EditAnywhere, Category = "Death|Flash")
	FName FlashEmissionParameter = TEXT("EmissionMultiplier");
};
