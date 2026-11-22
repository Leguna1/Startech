#pragma once

#include "CoreMinimal.h"
#include "TradeableData.h"
#include "WeaponGroup.h"
#include "WeaponData.generated.h"

UCLASS(Abstract, BlueprintType)
class STARTECH_API UWeaponData : public UTradeableData
{
	GENERATED_BODY()

public:
	virtual EWeaponGroup GetGroup() const PURE_VIRTUAL(UWeaponData::GetGroup, return EWeaponGroup::Laser;);

	// Every shot or beam reaches exactly this far, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	float Range = 15000.f;
};
