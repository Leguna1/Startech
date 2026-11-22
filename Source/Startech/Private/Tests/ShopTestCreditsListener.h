#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ShopTestCreditsListener.generated.h"

/**
 * Test helper for Startech.Shop.*: counts UInventoryComponent::OnCreditsChanged broadcasts (a dynamic delegate, so it
 * needs a UFUNCTION). Used only by the automation tests.
 */
UCLASS(Transient)
class UShopTestCreditsListener : public UObject
{
	GENERATED_BODY()

public:
	/** OnCreditsChanged broadcasts seen. */
	int32 Count = 0;

	/** Counts one broadcast. */
	UFUNCTION()
	void HandleCreditsChanged(int32 NewTotal, int32 Delta) { ++Count; }
};
