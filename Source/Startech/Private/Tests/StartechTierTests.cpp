#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AmmoData.h"
#include "ItemData.h"
#include "LootTable.h"
#include "StartechGameplayTags.h"

namespace StartechTierTests
{
	/** A transient mineral with Tag. */
	static UItemData* MakeMineral(const FGameplayTag& Tag)
	{
		UItemData* const Item = NewObject<UItemData>(GetTransientPackage());
		Item->ItemTag = Tag;
		return Item;
	}

	/** Weight for Tag. */
	static FTierMineralWeight Weight(const FGameplayTag& Tag, const float Value)
	{
		FTierMineralWeight Entry;
		Entry.Mineral = Tag;
		Entry.Weight = Value;
		return Entry;
	}

	/** The count of Item in Bundle. */
	static int32 CountOf(const FLootBundle& Bundle, const UItemData* const Item)
	{
		const FItemStack* const Stack = Bundle.Items.FindByPredicate([Item](const FItemStack& S) { return S.Item == Item; });
		return Stack ? Stack->Count : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechTierLootTest, "Startech.Tiers.LootRoll",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

/** Tier scaling of credits and amounts, and minerals picked by the tier's weights. */
bool FStartechTierLootTest::RunTest(const FString& Parameters)
{
	using namespace StartechTierTests;

	UItemData* const Ferrite = MakeMineral(StartechTags::Item_Mineral_Ferrite);
	UItemData* const Cobaltine = MakeMineral(StartechTags::Item_Mineral_Cobaltine);
	UItemData* const Aurium = MakeMineral(StartechTags::Item_Mineral_Aurium);
	UAmmoData* const Ammo = NewObject<UAmmoData>(GetTransientPackage());

	ULootTable* const Table = NewObject<ULootTable>(GetTransientPackage());
	Table->CreditChance = 1.f;
	Table->CreditsMin = 10;
	Table->CreditsMax = 10;
	Table->MineralChance = 1.f;
	const auto AddItem = [Table](UItemData* const Item, const float Chance, const int32 Count)
	{
		FLootItemEntry& Entry = Table->Items.AddDefaulted_GetRef();
		Entry.Item = Item;
		Entry.Chance = Chance;
		Entry.MinCount = Count;
		Entry.MaxCount = Count;
	};
	AddItem(Ferrite, 0.7f, 3);
	AddItem(Cobaltine, 0.3f, 2);
	AddItem(Aurium, 0.1f, 1);
	FLootAmmoEntry& AmmoEntry = Table->Ammo.AddDefaulted_GetRef();
	AmmoEntry.Ammo = Ammo;
	AmmoEntry.Chance = 1.f;
	AmmoEntry.MinCount = 1;
	AmmoEntry.MaxCount = 1;

	// Neutral: the old per-entry rolls, unscaled.
	const FLootBundle Plain = Table->Roll();
	TestEqual(TEXT("Neutral credits"), Plain.Credits, 10);

	// Tier II as an enemy drop: credits x1.8, amounts x1.5 (1 ammo -> 2, rounded).
	FSectorTierData TierII;
	TierII.KillCreditsMultiplier = 1.8f;
	TierII.LootAmountMultiplier = 1.5f;
	TierII.MineralWeights = { Weight(StartechTags::Item_Mineral_Ferrite, 60.f), Weight(StartechTags::Item_Mineral_Cobaltine, 40.f) };
	const FLootBundle Drop = Table->Roll(FLootRollModifiers::FromTier(TierII, ELootRollSource::EnemyDrop));
	TestEqual(TEXT("Enemy drop credits use the kill multiplier"), Drop.Credits, 18);
	TestEqual(TEXT("Ammo scaled and rounded"), Drop.Ammo.Num() == 1 ? static_cast<int32>(Drop.Ammo[0].Count) : 0, 2);
	TestEqual(TEXT("Exactly one mineral stack"), Drop.Items.Num(), static_cast<int32>(1));

	const FLootBundle World = Table->Roll(FLootRollModifiers::FromTier(TierII, ELootRollSource::World));
	TestEqual(TEXT("World loot credits use the loot multiplier"), World.Credits, 15);

	// A small multiplier still gives at least 1.
	FSectorTierData Tiny;
	Tiny.LootAmountMultiplier = 0.01f;
	TestEqual(TEXT("At least 1"), Table->Roll(FLootRollModifiers::FromTier(Tiny, ELootRollSource::World)).Credits, 1);

	// Weights: tier I gives only Ferrite (3 -> 3), tier IV never Ferrite and Aurium more often than Cobaltine.
	FSectorTierData TierI;
	TierI.MineralWeights = { Weight(StartechTags::Item_Mineral_Ferrite, 100.f), Weight(StartechTags::Item_Mineral_Cobaltine, 0.f),
		Weight(StartechTags::Item_Mineral_Aurium, 0.f) };
	FSectorTierData TierIV;
	TierIV.LootAmountMultiplier = 3.2f;
	TierIV.MineralWeights = { Weight(StartechTags::Item_Mineral_Ferrite, 0.f), Weight(StartechTags::Item_Mineral_Cobaltine, 40.f),
		Weight(StartechTags::Item_Mineral_Aurium, 60.f) };

	int32 TierIOther = 0;
	int32 TierIFerrite = 0;
	int32 TierIVFerrite = 0;
	int32 TierIVCobaltine = 0;
	int32 TierIVAurium = 0;
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		const FLootBundle One = Table->Roll(FLootRollModifiers::FromTier(TierI, ELootRollSource::World));
		TierIFerrite += CountOf(One, Ferrite) == 3 ? 1 : 0;
		TierIOther += CountOf(One, Cobaltine) + CountOf(One, Aurium);

		const FLootBundle Four = Table->Roll(FLootRollModifiers::FromTier(TierIV, ELootRollSource::World));
		TierIVFerrite += CountOf(Four, Ferrite);
		TierIVCobaltine += CountOf(Four, Cobaltine) > 0 ? 1 : 0;
		TierIVAurium += CountOf(Four, Aurium) > 0 ? 1 : 0;
		if (CountOf(Four, Aurium) > 0)
		{
			TestEqual(TEXT("Aurium 1 x3.2 -> 3"), CountOf(Four, Aurium), 3);
		}
	}
	TestEqual(TEXT("Tier I: Ferrite every roll at MineralChance 1"), TierIFerrite, 1000);
	TestEqual(TEXT("Tier I: no other mineral"), TierIOther, 0);
	TestEqual(TEXT("Tier IV: no Ferrite"), TierIVFerrite, 0);
	TestEqual(TEXT("Tier IV: one mineral per roll"), TierIVCobaltine + TierIVAurium, 1000);
	TestTrue(TEXT("Tier IV: Aurium more common than Cobaltine"), TierIVAurium > TierIVCobaltine);
	AddInfo(FString::Printf(TEXT("Tier IV over 1000 rolls: Cobaltine %d, Aurium %d"), TierIVCobaltine, TierIVAurium));
	return true;
}

#endif
