#include "ShopQuantity.h"

#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "ShopQuantity"

const TArray<EShopQuantity>& FShopQuantity::All()
{
	static const TArray<EShopQuantity> Quantities = {
		EShopQuantity::One, EShopQuantity::Ten, EShopQuantity::Hundred, EShopQuantity::Thousand, EShopQuantity::Max };
	return Quantities;
}

int32 FShopQuantity::GetUnits(const EShopQuantity Quantity)
{
	switch (Quantity)
	{
	case EShopQuantity::One:      return 1;
	case EShopQuantity::Ten:      return 10;
	case EShopQuantity::Hundred:  return 100;
	case EShopQuantity::Thousand: return 1000;
	default:                      return 0;
	}
}

FText FShopQuantity::GetLabel(const EShopQuantity Quantity)
{
	if (Quantity == EShopQuantity::Max)
	{
		return LOCTEXT("Max", "Max");
	}
	return FText::Format(LOCTEXT("Times", "×{0}"), FText::AsNumber(GetUnits(Quantity)));
}

bool FShopQuantity::FromKey(const FKey& Key, EShopQuantity& OutQuantity)
{
	static const FKey RowKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	static const FKey PadKeys[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RowKeys); ++Index)
	{
		if (Key == RowKeys[Index] || Key == PadKeys[Index])
		{
			OutQuantity = All()[Index];
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
