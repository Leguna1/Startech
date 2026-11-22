#pragma once

#include "CoreMinimal.h"
#include "WeaponData.h"
#include "ShieldDrainWeaponData.generated.h"

UCLASS(BlueprintType)
class STARTECH_API UShieldDrainWeaponData : public UWeaponData
{
	GENERATED_BODY()

public:
	virtual EWeaponGroup GetGroup() const override { return EWeaponGroup::ShieldDrain; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield Drain", meta = (ClampMin = "0"))
	float ShieldDrainPerSecond = 15.f;

	// Rounds of drain ammo used per second of beam.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shield Drain", meta = (ClampMin = "0"))
	float AmmoPerSecond = 1.f;
};
