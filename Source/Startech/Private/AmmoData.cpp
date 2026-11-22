#include "AmmoData.h"

bool UAmmoData::GetDisplayColor(FLinearColor& OutColor) const
{
	OutColor = Color;
	return true;
}
