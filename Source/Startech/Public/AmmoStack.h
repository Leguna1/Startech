#pragma once

#include "CoreMinimal.h"
#include "AmmoStack.generated.h"

class UAmmoData;

USTRUCT(BlueprintType)
struct STARTECH_API FAmmoStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TObjectPtr<UAmmoData> Ammo = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0"))
	float Count = 0.f;
};
