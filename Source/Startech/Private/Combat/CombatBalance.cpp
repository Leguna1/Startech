#include "Combat/CombatBalance.h"

#include "AmmoData.h"
#include "EnemyData.h"
#include "ItemData.h"
#include "LootBundle.h"
#include "ShopData.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechCombatBalance, Log, All);

const UCombatBalance* UCombatBalance::Get()
{
	UCombatBalance* const Balance = UStartechSettings::Get().CombatBalance.LoadSynchronous();
	if (Balance)
	{
		// Read every shot: never let it be collected and loaded again.
		if (!Balance->IsRooted())
		{
			Balance->AddToRoot();
		}
	}
	else
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogStartechCombatBalance, Error, TEXT("UStartechSettings::CombatBalance is not set or missing: no free ammo, locks or tier drops."));
		}
	}
	return Balance;
}

bool UCombatBalance::HasAmmoTag(const UTradeableData* const Good, const TArray<FGameplayTag>& Tags)
{
	const UAmmoData* const Ammo = Cast<UAmmoData>(Good);
	return Ammo && Ammo->ItemTag.IsValid() && Tags.ContainsByPredicate([Ammo](const FGameplayTag& Tag)
	{
		return Ammo->ItemTag.MatchesTagExact(Tag);
	});
}

bool UCombatBalance::IsFreeAmmo(const UTradeableData* const Good) const
{
	return HasAmmoTag(Good, { FreeAmmoTag });
}

bool UCombatBalance::IsDropOnly(const UTradeableData* const Good) const
{
	return HasAmmoTag(Good, DropOnlyAmmoTags);
}

UAmmoData* UCombatBalance::FindFreeAmmo() const
{
	return FreeAmmoTag.IsValid() ? UStartechSettings::Get().FindItemOfType<UAmmoData>(FreeAmmoTag) : nullptr;
}

bool UCombatBalance::IsAmmoLocked(const FGameplayTag& AmmoTag, const int32 HighestTier, FText& OutReason) const
{
	const FAmmoUnlock* const Unlock = AmmoUnlocks.FindByPredicate([&AmmoTag](const FAmmoUnlock& Each)
	{
		return AmmoTag.IsValid() && Each.AmmoTag.MatchesTagExact(AmmoTag);
	});
	if (!Unlock || HighestTier >= Unlock->RequiredTier)
	{
		return false;
	}
	OutReason = Unlock->LockedReason;
	return true;
}

void UCombatBalance::RollTierAmmoDrops(const int32 Tier, FLootBundle& Bundle) const
{
	const UStartechSettings& Settings = UStartechSettings::Get();
	for (const FTierAmmoDrop& Drop : TierAmmoDrops)
	{
		if (Drop.Tier != Tier || Drop.Chance <= 0.f || FMath::FRand() >= Drop.Chance)
		{
			continue;
		}
		UAmmoData* const DropAmmo = Settings.FindItemOfType<UAmmoData>(Drop.AmmoTag);
		if (!DropAmmo)
		{
			UE_LOG(LogStartechCombatBalance, Warning, TEXT("CombatBalance: tier ammo drop %s is not in the item catalog; skipped."),
				*Drop.AmmoTag.ToString());
			continue;
		}
		const int32 Count = FMath::RandRange(Drop.MinCount, FMath::Max(Drop.MinCount, Drop.MaxCount));
		if (FAmmoStack* const Existing = Bundle.Ammo.FindByPredicate([DropAmmo](const FAmmoStack& Stack) { return Stack.Ammo == DropAmmo; }))
		{
			Existing->Count += Count;
		}
		else
		{
			FAmmoStack& Stack = Bundle.Ammo.AddDefaulted_GetRef();
			Stack.Ammo = DropAmmo;
			Stack.Count = static_cast<float>(Count);
		}
	}
}

const FAmmoBalance* UCombatBalance::FindAmmo(const UAmmoData* const AmmoData) const
{
	return AmmoData && AmmoData->ItemTag.IsValid()
		? Ammo.FindByPredicate([AmmoData](const FAmmoBalance& Each) { return Each.AmmoTag.MatchesTagExact(AmmoData->ItemTag); })
		: nullptr;
}

const FEnemyBalance* UCombatBalance::FindEnemy(const UEnemyData* const Data) const
{
	return Data
		? Enemies.FindByPredicate([Data](const FEnemyBalance& Each) { return Data->EnemyTags.HasTagExact(Each.EnemyTag); })
		: nullptr;
}

float UCombatBalance::GetDamageMultiplier(const UAmmoData* const AmmoData)
{
	if (!AmmoData)
	{
		return 1.f;
	}
	const UCombatBalance* const Balance = Get();
	const FAmmoBalance* const Entry = Balance ? Balance->FindAmmo(AmmoData) : nullptr;
	return Entry ? Entry->DamageMultiplier : AmmoData->DamageMultiplier;
}

double UCombatBalance::GetBaseValue(const UTradeableData& Good)
{
	const UAmmoData* const AmmoData = Cast<UAmmoData>(&Good);
	const UCombatBalance* const Balance = AmmoData ? Get() : nullptr;
	const FAmmoBalance* const Entry = Balance ? Balance->FindAmmo(AmmoData) : nullptr;
	return Entry ? Entry->BaseValue : Good.BaseValue;
}

double UCombatBalance::GetRoundCost(const UAmmoData* const AmmoData) const
{
	if (!AmmoData || IsFreeAmmo(AmmoData) || IsDropOnly(AmmoData))
	{
		return 0.0;
	}
	const UShopData* const Shop = AmmoShop.LoadSynchronous();
	const bool bSold = Shop && Shop->Entries.ContainsByPredicate([AmmoData](const FShopEntry& Entry)
	{
		return Entry.Good == AmmoData && Entry.bShopSells;
	});
	return bSold ? Shop->GetPricePlayerPays(*AmmoData) : 0.0;
}

double UCombatBalance::GetSellValue(const UItemData* const Item) const
{
	const UShopData* const Shop = Item ? LootShop.LoadSynchronous() : nullptr;
	const bool bBought = Shop && Shop->Entries.ContainsByPredicate([Item](const FShopEntry& Entry)
	{
		return Entry.Good == Item && Entry.bShopBuys;
	});
	return bBought ? Shop->GetPricePlayerReceives(*Item) : 0.0;
}

double UCombatBalance::GetBundleValue(const FLootBundle& Bundle) const
{
	double Value = Bundle.Credits;
	for (const FItemStack& Stack : Bundle.Items)
	{
		Value += Stack.Count * GetSellValue(Stack.Item);
	}
	return Value;
}
