#pragma once

#include "CoreMinimal.h"
#include "WeaponStack.generated.h"

class UWeaponData;

// Spare weapons of one kind, held in the inventory and not fitted to any mount.
USTRUCT(BlueprintType)
struct STARTECH_API FWeaponStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponData> Weapon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	int32 Count = 0;
};
