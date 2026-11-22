#include "ShopData.h"

#include "Combat/CombatBalance.h"
#include "TradeableData.h"

double UShopData::GetPricePlayerPays(const UTradeableData& Good) const
{
	return FMath::Max(UCombatBalance::GetBaseValue(Good) * SellMarkup, 0.0);
}

double UShopData::GetPricePlayerReceives(const UTradeableData& Good) const
{
	return FMath::Max(UCombatBalance::GetBaseValue(Good) * BuyRate, 0.0);
}
