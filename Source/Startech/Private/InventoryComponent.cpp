#include "InventoryComponent.h"

#include "ItemData.h"
#include "StartechHUDLog.h"
#include "WeaponData.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::AddCredits(const int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	// Saturates rather than wrapping round to negative.
	const int32 Before = Credits;
	Credits = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Credits) + Amount, MAX_int32));
	if (Credits != Before)
	{
		NotifyCreditsChanged(Credits - Before);
	}
}

bool UInventoryComponent::SpendCredits(const int32 Amount)
{
	if (Amount < 0 || Amount > Credits)
	{
		return false;
	}

	if (Amount > 0)
	{
		Credits -= Amount;
		NotifyCreditsChanged(-Amount);
	}
	return true;
}

int32 UInventoryComponent::AddItem(UItemData* const Item, const int32 Count, const bool bShowInPickupFeed)
{
	const int32 Added = Item ? FMath::Min(Count, GetCargoFree()) : 0;
	if (Added <= 0)
	{
		return 0;
	}

	if (const int32 Index = FindStackIndex(Item); Index != INDEX_NONE)
	{
		Items[Index].Count += Added;
	}
	else
	{
		FItemStack& Stack = Items.AddDefaulted_GetRef();
		Stack.Item = Item;
		Stack.Count = Added;
	}

	OnItemsChanged.Broadcast();
	NotifyCargoChanged();

	if (bShowInPickupFeed)
	{
		STARTECH_HUD_LOG(TEXT("%s OnItemAdded: %s x%d"), *GetNameSafe(GetOwner()), *Item->DisplayName.ToString(), Added);
		OnItemAdded.Broadcast(Item->DisplayName, Added);
	}
	OnItemCountChanged.Broadcast(Item, Added);
	return Added;
}

bool UInventoryComponent::RemoveItem(const UItemData* const Item, const int32 Count)
{
	const int32 Index = FindStackIndex(Item);
	if (Index == INDEX_NONE || Count < 0 || Items[Index].Count < Count)
	{
		return false;
	}

	if (Count > 0)
	{
		Items[Index].Count -= Count;
		if (Items[Index].Count <= 0)
		{
			Items.RemoveAt(Index);
		}
		OnItemsChanged.Broadcast();
		NotifyCargoChanged();
		OnItemCountChanged.Broadcast(Item, -Count);
	}
	return true;
}

void UInventoryComponent::SetCargoCapacity(const int32 InCapacity)
{
	const int32 NewCapacity = FMath::Max(InCapacity, 0);
	if (NewCapacity == CargoCapacity)
	{
		return;
	}
	CargoCapacity = NewCapacity;
	NotifyCargoChanged();
}

void UInventoryComponent::NotifyCreditsChanged(const int32 Delta)
{
	STARTECH_HUD_LOG(TEXT("%s OnCreditsChanged: %d (%+d)"), *GetNameSafe(GetOwner()), Credits, Delta);
	OnCreditsChanged.Broadcast(Credits, Delta);
}

void UInventoryComponent::NotifyCargoChanged()
{
	const int32 Used = GetCargoUsed();
	STARTECH_HUD_LOG(TEXT("%s OnCargoChanged: %d / %d"), *GetNameSafe(GetOwner()), Used, CargoCapacity);
	OnCargoChanged.Broadcast(Used, CargoCapacity);
}

int32 UInventoryComponent::GetItemCount(const UItemData* const Item) const
{
	const int32 Index = FindStackIndex(Item);
	return Index != INDEX_NONE ? Items[Index].Count : 0;
}

int32 UInventoryComponent::GetCargoUsed() const
{
	int32 Used = 0;
	for (const FItemStack& Stack : Items)
	{
		Used += Stack.Count;
	}
	return Used;
}

void UInventoryComponent::AddWeapon(UWeaponData* const Weapon, const int32 Count)
{
	if (!Weapon || Count <= 0)
	{
		return;
	}

	if (const int32 Index = FindWeaponIndex(Weapon); Index != INDEX_NONE)
	{
		Weapons[Index].Count += Count;
	}
	else
	{
		FWeaponStack& Stack = Weapons.AddDefaulted_GetRef();
		Stack.Weapon = Weapon;
		Stack.Count = Count;
	}

	OnWeaponsChanged.Broadcast();
}

bool UInventoryComponent::RemoveWeapon(const UWeaponData* const Weapon, const int32 Count)
{
	const int32 Index = FindWeaponIndex(Weapon);
	if (Index == INDEX_NONE || Count < 0 || Weapons[Index].Count < Count)
	{
		return false;
	}

	if (Count > 0)
	{
		Weapons[Index].Count -= Count;
		if (Weapons[Index].Count <= 0)
		{
			Weapons.RemoveAt(Index);
		}
		OnWeaponsChanged.Broadcast();
	}
	return true;
}

int32 UInventoryComponent::GetWeaponCount(const UWeaponData* const Weapon) const
{
	const int32 Index = FindWeaponIndex(Weapon);
	return Index != INDEX_NONE ? Weapons[Index].Count : 0;
}

void UInventoryComponent::RestoreCredits(const int32 Amount)
{
	const int32 Before = Credits;
	Credits = FMath::Max(Amount, 0);
	NotifyCreditsChanged(Credits - Before);
}

void UInventoryComponent::RestoreItems(const TArray<FItemStack>& NewItems)
{
	Items.Reset();
	for (const FItemStack& Stack : NewItems)
	{
		if (!Stack.Item || Stack.Count <= 0)
		{
			continue;
		}
		if (const int32 Index = FindStackIndex(Stack.Item); Index != INDEX_NONE)
		{
			Items[Index].Count += Stack.Count;
		}
		else
		{
			Items.Add(Stack);
		}
	}

	if (GetCargoUsed() > CargoCapacity)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: restored cargo (%d) exceeds the capacity (%d)."), *GetNameSafe(GetOwner()),
			GetCargoUsed(), CargoCapacity);
	}

	OnItemsChanged.Broadcast();
	NotifyCargoChanged();
}

void UInventoryComponent::RestoreWeapons(const TArray<FWeaponStack>& NewWeapons)
{
	Weapons.Reset();
	for (const FWeaponStack& Stack : NewWeapons)
	{
		if (!Stack.Weapon || Stack.Count <= 0)
		{
			continue;
		}
		if (const int32 Index = FindWeaponIndex(Stack.Weapon); Index != INDEX_NONE)
		{
			Weapons[Index].Count += Stack.Count;
		}
		else
		{
			Weapons.Add(Stack);
		}
	}
	OnWeaponsChanged.Broadcast();
}

int32 UInventoryComponent::FindStackIndex(const UItemData* const Item) const
{
	if (!Item)
	{
		return INDEX_NONE;
	}
	return Items.IndexOfByPredicate([Item](const FItemStack& Stack) { return Stack.Item == Item; });
}

int32 UInventoryComponent::FindWeaponIndex(const UWeaponData* const Weapon) const
{
	if (!Weapon)
	{
		return INDEX_NONE;
	}
	return Weapons.IndexOfByPredicate([Weapon](const FWeaponStack& Stack) { return Stack.Weapon == Weapon; });
}
