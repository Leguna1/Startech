#pragma once

#include "CoreMinimal.h"
#include "ShopQuantity.generated.h"

/** The shop's bulk quantity: one selector for the whole shop, applied to every Buy and Sell button. */
UENUM(BlueprintType)
enum class EShopQuantity : uint8
{
	One,
	Ten,
	Hundred,
	Thousand,
	/** As many as possible: buying, what you can afford and fit; selling, everything held. */
	Max,
};

/** Helpers for EShopQuantity. */
struct STARTECH_API FShopQuantity
{
	/** Every quantity, in chip order (keys 1-5). */
	static const TArray<EShopQuantity>& All();

	/** The units for a fixed quantity (1, 10, 100, 1,000); 0 for Max, which depends on the row. */
	static int32 GetUnits(EShopQuantity Quantity);

	/** The chip label: "×1", "×10", "×100", "×1,000", "Max". */
	static FText GetLabel(EShopQuantity Quantity);

	/** The quantity for a shortcut key (1-5 on the number row or the keypad), or false. */
	static bool FromKey(const FKey& Key, EShopQuantity& OutQuantity);
};
