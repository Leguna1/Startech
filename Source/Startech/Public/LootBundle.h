#pragma once

#include "CoreMinimal.h"
#include "AmmoStack.h"
#include "ItemStack.h"
#include "LootBundle.generated.h"

// Credits, items and ammo handed over together.
USTRUCT(BlueprintType)
struct STARTECH_API FLootBundle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 Credits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FItemStack> Items;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FAmmoStack> Ammo;

	// True when there is nothing worth handing over: no credits, and no stack with an asset and a count.
	bool IsEmpty() const;
};
