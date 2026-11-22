#pragma once

#include "CoreMinimal.h"
#include "TradeableData.h"
#include "WeaponGroup.h"
#include "AmmoData.generated.h"

class UNiagaraSystem;
class USoundBase;

UCLASS(BlueprintType)
class STARTECH_API UAmmoData : public UTradeableData
{
	GENERATED_BODY()

public:
	virtual bool GetDisplayColor(FLinearColor& OutColor) const override;

	// The fire mode that uses this ammo.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	EWeaponGroup Group = EWeaponGroup::Laser;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.f;

	/**
	 * The tier the action bar draws when the ammo has no Icon: that many vertical bars for laser ammo, that many
	 * chevrons for shield drain ammo (LL1 1 ... LL4 4, SD1 1, SD2 2).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals", meta = (ClampMin = "1", ClampMax = "6"))
	int32 IconTier = 1;

	// Shield drain only: this fraction of the shield the beam actually removes goes to the firing ship's shield.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0", ClampMax = "1"))
	float ShieldTransferRatio = 0.f;

	// The look of every shot fired with this ammo, and its name on the HUD.
	// Effects that expose a "User.Color" linear color get this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor Color = FLinearColor::White;

	// Every effect and sound is optional; an empty slot is skipped.

	// At the muzzle when a laser fires, or once when a beam starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UNiagaraSystem> StartEffect;

	// Laser: the bolt, moved along its path facing its direction of travel.
	// Shield drain: the continuous beam, which may expose "User.BeamEnd" (world-space vector).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UNiagaraSystem> TravelEffect;

	// Laser: once at the impact. Shield drain: held at the beam's end point while the beam hits something.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UNiagaraSystem> HitEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> StartSound;

	// Follows the bolt, or loops while the beam is on. Stopped when either ends, so it may loop.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> TravelSound;

	// Laser only: at the impact.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> HitSound;
};
