#pragma once

#include "CoreMinimal.h"
#include "ChargedShotTypes.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * The look and sound hooks of the ranged enemy's charged shot (UStartechSettings::ChargedShot). Every effect is optional:
 * an empty one plays nothing. The numbers that change the fight are in DA_CombatBalance (FEnemyBehaviourBalance).
 */
USTRUCT(BlueprintType)
struct STARTECH_API FChargedShotEffects
{
	GENERATED_BODY()

	/** The hull's glow during the telegraph: its colours blend towards this, up to GlowMaxAmount at the end. */
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FLinearColor GlowColor = FLinearColor(1.f, 0.08f, 0.02f);

	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowMaxAmount = 0.8f;

	/** The emission parameter × (1 + this × the glow). */
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0"))
	float GlowEmissionBoost = 8.f;

	/** Glow pulse frequency, rising to GlowPulseHzEnd as the shot nears. */
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0", Units = "Hz"))
	float GlowPulseHzStart = 3.f;

	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0", Units = "Hz"))
	float GlowPulseHzEnd = 9.f;

	/** Vector parameters the glow tints, and the scalar it raises (as the death flash). */
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	TArray<FName> GlowColorParameters = { TEXT("Color 1"), TEXT("Color 2"), TEXT("Color 3") };

	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FName GlowEmissionParameter = TEXT("EmissionMultiplier");

	/** Attached at the muzzle for the telegraph. */
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	TSoftObjectPtr<UNiagaraSystem> ChargeFX;

	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.01"))
	float ChargeFXScale = 1.f;

	/** Played at the muzzle when the telegraph starts. */
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	TSoftObjectPtr<USoundBase> ChargeSound;

	/** Attached to the projectile while it flies. */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	TSoftObjectPtr<UNiagaraSystem> ProjectileFX;

	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.01"))
	float ProjectileFXScale = 1.f;

	/** Played at the muzzle when the shot leaves. */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	TSoftObjectPtr<USoundBase> FireSound;

	/** Spawned where the projectile hits anything. */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	TSoftObjectPtr<UNiagaraSystem> ImpactFX;

	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.01"))
	float ImpactFXScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	TSoftObjectPtr<USoundBase> ImpactSound;
};
