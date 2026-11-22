#pragma once

#include "CoreMinimal.h"
#include "TradeableData.h"
#include "ItemData.generated.h"

// A mineral type that can be carried as cargo and sold. Name and value live in UTradeableData.
UCLASS(BlueprintType)
class STARTECH_API UItemData : public UTradeableData
{
	GENERATED_BODY()

public:
	virtual bool GetDisplayColor(FLinearColor& OutColor) const override;

	// Tints the mineral in the UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FLinearColor Color = FLinearColor::White;
};
