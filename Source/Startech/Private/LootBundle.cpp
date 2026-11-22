#include "LootBundle.h"

bool FLootBundle::IsEmpty() const
{
	const bool bHasItems = Items.ContainsByPredicate([](const FItemStack& Stack)
	{
		return Stack.Item && Stack.Count > 0;
	});
	const bool bHasAmmo = Ammo.ContainsByPredicate([](const FAmmoStack& Stack)
	{
		return Stack.Ammo && Stack.Count > 0.f;
	});

	return Credits <= 0 && !bHasItems && !bHasAmmo;
}
