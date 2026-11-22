#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShopData.generated.h"

class UTradeableData;

USTRUCT(BlueprintType)
struct STARTECH_API FShopEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UTradeableData> Good = nullptr;

	// The player can buy this here.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	bool bShopSells = true;

	// The player can sell this here. Ignored for ammo, which is never bought back.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	bool bShopBuys = true;
};

// One shop at a station: what it trades and at what margins.
UCLASS(BlueprintType)
class STARTECH_API UShopData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Credits per unit the player pays here: the base value (UCombatBalance::GetBaseValue) × SellMarkup. May be
	 * fractional; only a trade's total is rounded (up), by FShopTrading.
	 */
	double GetPricePlayerPays(const UTradeableData& Good) const;

	/**
	 * Credits per unit the player receives here: the base value (UCombatBalance::GetBaseValue) × BuyRate. May be
	 * fractional; only a trade's total is rounded (down), by FShopTrading.
	 */
	double GetPricePlayerReceives(const UTradeableData& Good) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	float SellMarkup = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	float BuyRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<FShopEntry> Entries;
};
