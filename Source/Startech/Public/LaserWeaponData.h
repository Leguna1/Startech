#pragma once

#include "CoreMinimal.h"
#include "WeaponData.h"
#include "LaserWeaponData.generated.h"

UCLASS(BlueprintType)
class STARTECH_API ULaserWeaponData : public UWeaponData
{
	GENERATED_BODY()

public:
	virtual EWeaponGroup GetGroup() const override { return EWeaponGroup::Laser; }

	// Per shot, before the ammo's multiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "0"))
	float Damage = 10.f;

	// Shots per second.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "0"))
	float FireRate = 4.f;

	// Bolts fly at this speed and deal their damage when they arrive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Laser", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float BoltSpeed = 40000.f;
};
