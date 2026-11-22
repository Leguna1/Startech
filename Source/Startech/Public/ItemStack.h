#pragma once

#include "CoreMinimal.h"
#include "ItemStack.generated.h"

class UItemData;

USTRUCT(BlueprintType)
struct STARTECH_API FItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Count = 0;
};
