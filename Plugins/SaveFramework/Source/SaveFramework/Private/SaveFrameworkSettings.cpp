#include "SaveFrameworkSettings.h"

USaveFrameworkSettings::USaveFrameworkSettings()
{
	CategoryName = TEXT("Plugins");
}

const USaveFrameworkSettings& USaveFrameworkSettings::Get()
{
	return *GetDefault<USaveFrameworkSettings>();
}

FString USaveFrameworkSettings::MakeSlotName(const int32 SlotIndex) const
{
	return FString::Printf(TEXT("%s%d"), *SlotNamePrefix, SlotIndex);
}

bool USaveFrameworkSettings::IsValidSlotIndex(const int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < NumSlots;
}
