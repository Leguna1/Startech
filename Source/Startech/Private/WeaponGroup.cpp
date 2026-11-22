#include "WeaponGroup.h"

TArray<EWeaponGroup> FWeaponGroups::All()
{
	TArray<EWeaponGroup> Groups;
	const UEnum* const Enum = StaticEnum<EWeaponGroup>();
	// NumEnums includes the hidden _MAX entry.
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		Groups.Add(static_cast<EWeaponGroup>(Enum->GetValueByIndex(Index)));
	}
	return Groups;
}
