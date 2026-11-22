#include "ItemData.h"

bool UItemData::GetDisplayColor(FLinearColor& OutColor) const
{
	OutColor = Color;
	return true;
}
