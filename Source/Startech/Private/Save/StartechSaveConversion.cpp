#include "Save/StartechSaveConversion.h"

#include "AmmoData.h"
#include "ItemData.h"
#include "Save/StartechSaveLog.h"
#include "StartechSettings.h"
#include "TradeableData.h"
#include "WeaponData.h"

template <typename T>
T* FStartechSaveConversion::Resolve(const FGameplayTag& Tag, const TCHAR* const Context)
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}
	UTradeableData* const Good = UStartechSettings::Get().FindItem(Tag);
	T* const Typed = Cast<T>(Good);
	if (!Typed)
	{
		UE_LOG(LogStartechSave, Warning, TEXT("%s: %s is %s; skipped."), Context, *Tag.ToString(),
			Good ? TEXT("not the expected kind of good") : TEXT("not in the item catalog"));
	}
	return Typed;
}

template UItemData* FStartechSaveConversion::Resolve<UItemData>(const FGameplayTag&, const TCHAR*);
template UWeaponData* FStartechSaveConversion::Resolve<UWeaponData>(const FGameplayTag&, const TCHAR*);
template UAmmoData* FStartechSaveConversion::Resolve<UAmmoData>(const FGameplayTag&, const TCHAR*);

FGameplayTag FStartechSaveConversion::TagOf(const UTradeableData* const Good, const TCHAR* const Context)
{
	if (!Good)
	{
		return FGameplayTag();
	}
	if (!Good->ItemTag.IsValid())
	{
		UE_LOG(LogStartechSave, Warning, TEXT("%s: %s has no ItemTag and cannot be saved."), Context, *Good->GetName());
	}
	return Good->ItemTag;
}

TArray<FStartechTaggedCount> FStartechSaveConversion::ToTagged(const TArray<FItemStack>& Stacks, const TCHAR* const Context)
{
	TArray<FStartechTaggedCount> Entries;
	for (const FItemStack& Stack : Stacks)
	{
		const FGameplayTag Tag = TagOf(Stack.Item, Context);
		if (Tag.IsValid() && Stack.Count > 0)
		{
			FStartechTaggedCount& Entry = Entries.AddDefaulted_GetRef();
			Entry.ItemTag = Tag;
			Entry.Count = Stack.Count;
		}
	}
	return Entries;
}

TArray<FStartechTaggedCount> FStartechSaveConversion::ToTagged(const TArray<FWeaponStack>& Stacks, const TCHAR* const Context)
{
	TArray<FStartechTaggedCount> Entries;
	for (const FWeaponStack& Stack : Stacks)
	{
		const FGameplayTag Tag = TagOf(Stack.Weapon, Context);
		if (Tag.IsValid() && Stack.Count > 0)
		{
			FStartechTaggedCount& Entry = Entries.AddDefaulted_GetRef();
			Entry.ItemTag = Tag;
			Entry.Count = Stack.Count;
		}
	}
	return Entries;
}

TArray<FStartechTaggedAmount> FStartechSaveConversion::ToTagged(const TArray<FAmmoStack>& Stacks, const TCHAR* const Context)
{
	TArray<FStartechTaggedAmount> Entries;
	for (const FAmmoStack& Stack : Stacks)
	{
		const FGameplayTag Tag = TagOf(Stack.Ammo, Context);
		if (Tag.IsValid() && Stack.Count > 0.f)
		{
			FStartechTaggedAmount& Entry = Entries.AddDefaulted_GetRef();
			Entry.ItemTag = Tag;
			Entry.Amount = Stack.Count;
		}
	}
	return Entries;
}

TArray<FItemStack> FStartechSaveConversion::ToItemStacks(const TArray<FStartechTaggedCount>& Entries, const TCHAR* const Context)
{
	TArray<FItemStack> Stacks;
	for (const FStartechTaggedCount& Entry : Entries)
	{
		if (UItemData* const Item = Resolve<UItemData>(Entry.ItemTag, Context))
		{
			FItemStack& Stack = Stacks.AddDefaulted_GetRef();
			Stack.Item = Item;
			Stack.Count = Entry.Count;
		}
	}
	return Stacks;
}

TArray<FWeaponStack> FStartechSaveConversion::ToWeaponStacks(const TArray<FStartechTaggedCount>& Entries, const TCHAR* const Context)
{
	TArray<FWeaponStack> Stacks;
	for (const FStartechTaggedCount& Entry : Entries)
	{
		if (UWeaponData* const Weapon = Resolve<UWeaponData>(Entry.ItemTag, Context))
		{
			FWeaponStack& Stack = Stacks.AddDefaulted_GetRef();
			Stack.Weapon = Weapon;
			Stack.Count = Entry.Count;
		}
	}
	return Stacks;
}

TArray<FAmmoStack> FStartechSaveConversion::ToAmmoStacks(const TArray<FStartechTaggedAmount>& Entries, const TCHAR* const Context)
{
	TArray<FAmmoStack> Stacks;
	for (const FStartechTaggedAmount& Entry : Entries)
	{
		if (UAmmoData* const Ammo = Resolve<UAmmoData>(Entry.ItemTag, Context))
		{
			FAmmoStack& Stack = Stacks.AddDefaulted_GetRef();
			Stack.Ammo = Ammo;
			Stack.Count = Entry.Amount;
		}
	}
	return Stacks;
}
