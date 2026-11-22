#include "ShipFitting.h"

#include "GameFramework/Actor.h"
#include "InventoryComponent.h"
#include "WeaponData.h"
#include "WeaponMountComponent.h"
#include "WeaponSystemComponent.h"

bool FShipFitting::FitWeapon(AActor& Ship, UWeaponMountComponent& Mount, UWeaponData* const NewWeapon)
{
	UInventoryComponent* const Inventory = Ship.FindComponentByClass<UInventoryComponent>();
	if (!Inventory || Mount.GetOwner() != &Ship || !Mount.CanFit(NewWeapon) || Inventory->GetWeaponCount(NewWeapon) < 1)
	{
		return false;
	}

	UWeaponData* const OldWeapon = Mount.GetFittedWeapon();

	// The mount changes first, so listeners to the inventory's change see the finished swap.
	Mount.SetFittedWeapon(NewWeapon);
	Inventory->RemoveWeapon(NewWeapon, 1);
	if (OldWeapon)
	{
		Inventory->AddWeapon(OldWeapon, 1);
	}
	return true;
}

bool FShipFitting::RestoreWeapon(AActor& Ship, UWeaponMountComponent& Mount, UWeaponData* const Weapon)
{
	if (Mount.GetOwner() != &Ship || (Weapon && !Mount.CanFit(Weapon)))
	{
		return false;
	}

	Mount.SetFittedWeapon(Weapon);
	return true;
}

UWeaponData* FShipFitting::GetNextFittable(const AActor& Ship, const UWeaponMountComponent& Mount)
{
	const UInventoryComponent* const Inventory = Ship.FindComponentByClass<UInventoryComponent>();
	if (!Inventory || Mount.GetOwner() != &Ship)
	{
		return nullptr;
	}

	const TArray<FWeaponStack>& Spares = Inventory->GetWeapons();
	const UWeaponData* const Fitted = Mount.GetFittedWeapon();

	// Start after the fitted weapon's stack, or at the front if it has no spares.
	const int32 FittedIndex = Fitted
		? Spares.IndexOfByPredicate([Fitted](const FWeaponStack& Stack) { return Stack.Weapon == Fitted; })
		: INDEX_NONE;
	const int32 First = FittedIndex + 1;

	for (int32 Offset = 0; Offset < Spares.Num(); ++Offset)
	{
		const FWeaponStack& Stack = Spares[(First + Offset) % Spares.Num()];
		if (Stack.Count > 0 && Stack.Weapon != Fitted && Mount.CanFit(Stack.Weapon))
		{
			return Stack.Weapon;
		}
	}
	return nullptr;
}
