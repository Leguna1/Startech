#include "LootTable.h"

#include "AmmoData.h"
#include "ItemData.h"
#include "StartechGameplayTags.h"

namespace
{
	// Zero when the chance fails, otherwise a count in [Min, Max]. Max below Min counts as Min.
	int32 RollCount(const float Chance, const int32 Min, const int32 Max)
	{
		// FRand can return exactly 1, so a chance of 1 is checked apart: it always succeeds.
		if (Chance <= 0.f || (Chance < 1.f && FMath::FRand() >= Chance))
		{
			return 0;
		}
		return FMath::RandRange(Min, FMath::Max(Min, Max));
	}

	// Count × Multiplier, rounded and at least 1; nothing stays nothing.
	int32 ScaleCount(const int32 Count, const float Multiplier)
	{
		if (Count <= 0)
		{
			return 0;
		}
		return FMath::Max(FMath::RoundToInt(static_cast<float>(Count) * FMath::Max(Multiplier, 0.f)), 1);
	}

	// True when Item is a mineral (its ItemTag is under Item.Mineral).
	bool IsMineral(const UItemData* const Item)
	{
		return Item && Item->ItemTag.MatchesTag(StartechTags::Item_Mineral);
	}

	// Item's weight in Weights, 0 when it isn't listed.
	float MineralWeight(const TArray<FTierMineralWeight>& Weights, const UItemData& Item)
	{
		const FTierMineralWeight* const Found = Weights.FindByPredicate([&Item](const FTierMineralWeight& Weight)
		{
			return Weight.Mineral.MatchesTagExact(Item.ItemTag);
		});
		return Found ? FMath::Max(Found->Weight, 0.f) : 0.f;
	}

	// Adds Count of Item to Bundle, sharing a stack with the same item.
	void AddItem(FLootBundle& Bundle, UItemData* const Item, const int32 Count)
	{
		if (FItemStack* const Existing = Bundle.Items.FindByPredicate([Item](const FItemStack& Stack) { return Stack.Item == Item; }))
		{
			Existing->Count += Count;
			return;
		}
		FItemStack& Stack = Bundle.Items.AddDefaulted_GetRef();
		Stack.Item = Item;
		Stack.Count = Count;
	}

	// Adds Count of Ammo to Bundle, sharing a stack with the same ammo.
	void AddAmmo(FLootBundle& Bundle, UAmmoData* const Ammo, const int32 Count)
	{
		if (FAmmoStack* const Existing = Bundle.Ammo.FindByPredicate([Ammo](const FAmmoStack& Stack) { return Stack.Ammo == Ammo; }))
		{
			Existing->Count += Count;
			return;
		}
		FAmmoStack& Stack = Bundle.Ammo.AddDefaulted_GetRef();
		Stack.Ammo = Ammo;
		Stack.Count = Count;
	}
}

FLootRollModifiers FLootRollModifiers::FromTier(const FSectorTierData& Tier, const ELootRollSource Source)
{
	FLootRollModifiers Modifiers;
	Modifiers.CreditsMultiplier = Source == ELootRollSource::EnemyDrop ? Tier.KillCreditsMultiplier : Tier.LootAmountMultiplier;
	Modifiers.AmountMultiplier = Tier.LootAmountMultiplier;
	Modifiers.MineralWeights = Tier.MineralWeights;
	return Modifiers;
}

FLootBundle ULootTable::Roll() const
{
	return Roll(FLootRollModifiers());
}

FLootBundle ULootTable::Roll(const FLootRollModifiers& Modifiers) const
{
	FLootBundle Bundle;
	Bundle.Credits = ScaleCount(RollCount(CreditChance, CreditsMin, CreditsMax), Modifiers.CreditsMultiplier);

	// The mineral entries the tier can pick, with their weights. Empty means each mineral entry rolls on its own.
	TArray<const FLootItemEntry*> WeightedMinerals;
	TArray<float> Weights;
	float TotalWeight = 0.f;
	for (const FLootItemEntry& Entry : Items)
	{
		const float Weight = IsMineral(Entry.Item) ? MineralWeight(Modifiers.MineralWeights, *Entry.Item) : 0.f;
		if (Weight > 0.f)
		{
			WeightedMinerals.Add(&Entry);
			Weights.Add(Weight);
			TotalWeight += Weight;
		}
	}
	const bool bWeightedMinerals = TotalWeight > 0.f;

	for (const FLootItemEntry& Entry : Items)
	{
		// With weights, minerals come only from the one weighted roll below.
		if (!Entry.Item || (bWeightedMinerals && IsMineral(Entry.Item)))
		{
			continue;
		}
		const int32 Count = ScaleCount(RollCount(Entry.Chance, Entry.MinCount, Entry.MaxCount), Modifiers.AmountMultiplier);
		if (Count > 0)
		{
			AddItem(Bundle, Entry.Item, Count);
		}
	}

	// FRand can return exactly 1, so a chance of 1 always succeeds.
	if (bWeightedMinerals && MineralChance > 0.f && (MineralChance >= 1.f || FMath::FRand() < MineralChance))
	{
		float Pick = FMath::FRand() * TotalWeight;
		int32 Chosen = WeightedMinerals.Num() - 1;
		for (int32 Index = 0; Index < WeightedMinerals.Num(); ++Index)
		{
			Pick -= Weights[Index];
			if (Pick < 0.f)
			{
				Chosen = Index;
				break;
			}
		}

		const FLootItemEntry& Entry = *WeightedMinerals[Chosen];
		const int32 Count = ScaleCount(FMath::RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount)),
			Modifiers.AmountMultiplier);
		if (Count > 0)
		{
			AddItem(Bundle, Entry.Item, Count);
		}
	}

	for (const FLootAmmoEntry& Entry : Ammo)
	{
		const int32 Count = Entry.Ammo
			? ScaleCount(RollCount(Entry.Chance, Entry.MinCount, Entry.MaxCount), Modifiers.AmountMultiplier)
			: 0;
		if (Count > 0)
		{
			AddAmmo(Bundle, Entry.Ammo, Count);
		}
	}

	return Bundle;
}
