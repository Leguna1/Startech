#pragma once

#include "CoreMinimal.h"
#include "AmmoStack.h"
#include "ItemStack.h"
#include "Save/StartechSaveTypes.h"
#include "WeaponStack.h"

class UTradeableData;

/**
 * Converts between the game's asset-based stacks and the tag-based save entries. Assets are resolved through
 * UStartechSettings::ItemCatalog. Anything that cannot be converted (an untagged asset, an unknown tag, a tag of the
 * wrong kind) is skipped with a warning naming Context, so a bad entry never breaks a whole save.
 */
struct FStartechSaveConversion
{
	/** Cargo stacks to tagged counts. */
	static TArray<FStartechTaggedCount> ToTagged(const TArray<FItemStack>& Stacks, const TCHAR* Context);

	/** Spare weapon stacks to tagged counts. */
	static TArray<FStartechTaggedCount> ToTagged(const TArray<FWeaponStack>& Stacks, const TCHAR* Context);

	/** Ammo stacks to tagged amounts. */
	static TArray<FStartechTaggedAmount> ToTagged(const TArray<FAmmoStack>& Stacks, const TCHAR* Context);

	/** Tagged counts to cargo stacks. */
	static TArray<FItemStack> ToItemStacks(const TArray<FStartechTaggedCount>& Entries, const TCHAR* Context);

	/** Tagged counts to spare weapon stacks. */
	static TArray<FWeaponStack> ToWeaponStacks(const TArray<FStartechTaggedCount>& Entries, const TCHAR* Context);

	/** Tagged amounts to ammo stacks. */
	static TArray<FAmmoStack> ToAmmoStacks(const TArray<FStartechTaggedAmount>& Entries, const TCHAR* Context);

	/** Good's ItemTag, or an empty tag (with a warning) when it has none. Null gives an empty tag silently. */
	static FGameplayTag TagOf(const UTradeableData* Good, const TCHAR* Context);

	/** The catalog good of type T tagged Tag, or null (with a warning) for an unknown tag or another kind of good. */
	template <typename T>
	static T* Resolve(const FGameplayTag& Tag, const TCHAR* Context);
};
