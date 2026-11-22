#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TradeableData.generated.h"

class UTexture2D;

// Anything a shop can list: minerals, ammo and weapons.
UCLASS(Abstract, BlueprintType)
class STARTECH_API UTradeableData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The colour the UI tints this good with. False when the good has none.
	virtual bool GetDisplayColor(FLinearColor& OutColor) const { return false; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade")
	FText DisplayName;

	/**
	 * Credits per unit before a shop's markup or buy rate. May be fractional (ammo below a credit a round); a trade
	 * rounds only its total (FShopTrading).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (ClampMin = "0"))
	float BaseValue = 1.f;

	/**
	 * The good's identity (Item.Mineral.<Name>, Item.Ammo.<Name> or Item.Weapon.<Name>): the subject of its
	 * Quest.Event.ItemCollected events, what Collect objectives ask for, the reward tag that grants it, and how saves
	 * store it. Unique per asset; list the asset in UStartechSettings::ItemCatalog so it can be found by tag.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trade", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	/** Optional icon for the action bar. When set it replaces the icon the bar draws in code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UTexture2D> Icon;
};
