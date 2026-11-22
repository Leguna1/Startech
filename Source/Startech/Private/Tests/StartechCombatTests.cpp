#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AmmoData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Combat/CombatBalance.h"
#include "Combat/CombatTTKTable.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LootBundle.h"
#include "ShopData.h"
#include "ShopTrading.h"
#include "StartechGameplayTags.h"
#include "StartechSettings.h"
#include "Tests/CombatTestAmmoListener.h"
#include "WeaponSystemComponent.h"

namespace StartechCombatTests
{
	/**
	 * A bare world holding one actor with a weapon system (no mounts, no BeginPlay), with transient LL1 (free), LL2
	 * and SD1 ammo. Destroys the world when it goes out of scope.
	 */
	struct FAmmoFixture
	{
		FAmmoFixture()
		{
			World = UWorld::CreateWorld(EWorldType::None, false, TEXT("StartechCombatTestWorld"));
			Owner = World->SpawnActor<AActor>();
			Weapons = NewObject<UWeaponSystemComponent>(Owner, TEXT("WeaponSystem"));
			Weapons->RegisterComponent();

			LL1 = MakeAmmo(StartechTags::Item_Ammo_LL1, EWeaponGroup::Laser, 1.f);
			LL2 = MakeAmmo(StartechTags::Item_Ammo_LL2, EWeaponGroup::Laser, 2.f);
			SD1 = MakeAmmo(StartechTags::Item_Ammo_SD1, EWeaponGroup::ShieldDrain, 1.f);
			Weapons->SetFreeAmmo(LL1);

			Listener = NewObject<UCombatTestAmmoListener>(GetTransientPackage());
			Weapons->OnAmmoAutoSwitched.AddDynamic(Listener, &UCombatTestAmmoListener::HandleAutoSwitched);
		}

		~FAmmoFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		static UAmmoData* MakeAmmo(const FGameplayTag& Tag, const EWeaponGroup Group, const float Multiplier)
		{
			UAmmoData* const Ammo = NewObject<UAmmoData>(GetTransientPackage());
			Ammo->ItemTag = Tag;
			Ammo->Group = Group;
			Ammo->DamageMultiplier = Multiplier;
			Ammo->DisplayName = FText::FromString(Tag.ToString());
			return Ammo;
		}

		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UWeaponSystemComponent* Weapons = nullptr;
		UAmmoData* LL1 = nullptr;
		UAmmoData* LL2 = nullptr;
		UAmmoData* SD1 = nullptr;
		UCombatTestAmmoListener* Listener = nullptr;
	};

	/** Every UShopData asset in /Game, loaded. Scans first: -game and commandlets may not have scanned yet. */
	TArray<UShopData*> LoadAllShops()
	{
		IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		Registry.ScanPathsSynchronous({ TEXT("/Game") }, false);
		TArray<FAssetData> Found;
		Registry.GetAssetsByClass(UShopData::StaticClass()->GetClassPathName(), Found, true);

		TArray<UShopData*> Shops;
		for (const FAssetData& Asset : Found)
		{
			if (UShopData* const Shop = Cast<UShopData>(Asset.GetAsset()))
			{
				Shops.Add(Shop);
			}
		}
		return Shops;
	}
}

using namespace StartechCombatTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechCombatFreeAmmoTest, "Startech.Combat.FreeAmmoNeverConsumes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechCombatFreeAmmoTest::RunTest(const FString& Parameters)
{
	FAmmoFixture Fixture;
	UWeaponSystemComponent& Weapons = *Fixture.Weapons;

	TestTrue(TEXT("LL1 is unlimited"), Weapons.IsUnlimited(Fixture.LL1));
	TestTrue(TEXT("LL1 can fire with nothing held"), Weapons.HasAmmo(Fixture.LL1));
	TestFalse(TEXT("LL2 is not unlimited"), Weapons.IsUnlimited(Fixture.LL2));

	for (int32 Shot = 0; Shot < 1000; ++Shot)
	{
		if (!Weapons.ConsumeAmmo(Fixture.LL1, 1.f))
		{
			AddError(FString::Printf(TEXT("LL1 shot %d failed to fire"), Shot));
			break;
		}
	}
	TestTrue(TEXT("LL1 still fires after 1000 shots"), Weapons.HasAmmo(Fixture.LL1));

	// Never stored: adding, restoring a pre-free save or loot all leave no stack.
	Weapons.AddAmmo(Fixture.LL1, 500.f);
	TArray<FAmmoStack> Saved;
	Saved.Add({ Fixture.LL1, 750.f });
	Saved.Add({ Fixture.LL2, 40.f });
	Weapons.RestoreAmmo(Saved);
	TestEqual(TEXT("only LL2 is stored"), Weapons.GetAmmoStacks().Num(), 1);
	TestEqual(TEXT("LL2 restored"), FMath::RoundToInt(Weapons.GetAmmoCount(Fixture.LL2)), 40);
	TestEqual(TEXT("LL1 has no count"), Weapons.GetAmmoCount(Fixture.LL1), 0.f);

	// LL2 is still used up normally.
	TestTrue(TEXT("LL2 fires"), Weapons.ConsumeAmmo(Fixture.LL2, 1.f));
	TestEqual(TEXT("LL2 used"), FMath::RoundToInt(Weapons.GetAmmoCount(Fixture.LL2)), 39);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechCombatAutoSwitchTest, "Startech.Combat.AutoSwitchToFreeAmmo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechCombatAutoSwitchTest::RunTest(const FString& Parameters)
{
	FAmmoFixture Fixture;
	UWeaponSystemComponent& Weapons = *Fixture.Weapons;

	Weapons.AddAmmo(Fixture.LL2, 1.f);
	Weapons.SelectAmmo(Fixture.LL2);

	// Not firing: an empty selection is left alone.
	TestTrue(TEXT("last LL2 round fired"), Weapons.ConsumeAmmo(Fixture.LL2, 1.f));
	Weapons.UpdateFiring(false, FVector::ZeroVector, 0.016f);
	TestEqual(TEXT("no switch without the trigger"), Weapons.GetSelectedAmmo(), Fixture.LL2);
	TestEqual(TEXT("no notice without the trigger"), Fixture.Listener->Count, 0);

	// Firing: switches to LL1 in the same tick, once.
	Weapons.UpdateFiring(true, FVector::ZeroVector, 0.016f);
	TestEqual(TEXT("switched to LL1"), Weapons.GetSelectedAmmo(), Fixture.LL1);
	TestEqual(TEXT("LL1 selected for the laser group"), Weapons.GetSelectedAmmoForGroup(EWeaponGroup::Laser), Fixture.LL1);
	TestEqual(TEXT("one notice"), Fixture.Listener->Count, 1);
	TestEqual(TEXT("notice names LL2"), Fixture.Listener->Exhausted.Get(), Fixture.LL2);
	TestEqual(TEXT("notice names LL1"), Fixture.Listener->Replacement.Get(), Fixture.LL1);

	Weapons.UpdateFiring(true, FVector::ZeroVector, 0.016f);
	TestEqual(TEXT("LL1 never switches away"), Fixture.Listener->Count, 1);

	// Shield drain ammo running out is not switched.
	Weapons.SelectAmmo(Fixture.SD1);
	Weapons.UpdateFiring(true, FVector::ZeroVector, 0.016f);
	TestEqual(TEXT("empty SD1 stays selected"), Weapons.GetSelectedAmmo(), Fixture.SD1);
	TestEqual(TEXT("no notice for SD1"), Fixture.Listener->Count, 1);

	// The wheel skips empty LL2 but always finds LL1.
	Weapons.SelectAmmo(Fixture.LL2);
	Weapons.CycleAmmoInActiveGroup(1, { Fixture.LL1, Fixture.LL2 });
	TestEqual(TEXT("cycle lands on LL1"), Weapons.GetSelectedAmmo(), Fixture.LL1);
	Weapons.CycleAmmoInActiveGroup(1, { Fixture.LL1, Fixture.LL2 });
	TestEqual(TEXT("cycle skips empty LL2"), Weapons.GetSelectedAmmo(), Fixture.LL1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechCombatAmmoLockTest, "Startech.Combat.AmmoLock",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechCombatAmmoLockTest::RunTest(const FString& Parameters)
{
	// The rule itself, on a transient balance.
	UCombatBalance* const Transient = NewObject<UCombatBalance>(GetTransientPackage());
	FAmmoUnlock& Unlock = Transient->AmmoUnlocks.AddDefaulted_GetRef();
	Unlock.AmmoTag = StartechTags::Item_Ammo_LL3;
	Unlock.RequiredTier = 3;
	Unlock.LockedReason = FText::FromString(TEXT("Reach Glacier (tier III)"));

	FText Reason;
	TestTrue(TEXT("LL3 locked at tier I"), Transient->IsAmmoLocked(StartechTags::Item_Ammo_LL3, 1, Reason));
	TestTrue(TEXT("LL3 locked at tier II"), Transient->IsAmmoLocked(StartechTags::Item_Ammo_LL3, 2, Reason));
	TestEqual(TEXT("reason"), Reason.ToString(), FString(TEXT("Reach Glacier (tier III)")));
	TestFalse(TEXT("LL3 unlocked at tier III"), Transient->IsAmmoLocked(StartechTags::Item_Ammo_LL3, 3, Reason));
	TestFalse(TEXT("LL3 unlocked at tier IV"), Transient->IsAmmoLocked(StartechTags::Item_Ammo_LL3, 4, Reason));
	TestFalse(TEXT("LL2 never locked"), Transient->IsAmmoLocked(StartechTags::Item_Ammo_LL2, 1, Reason));

	// The shipped DA_CombatBalance.
	const UCombatBalance* const Balance = UCombatBalance::Get();
	if (!TestNotNull(TEXT("DA_CombatBalance set in UStartechSettings"), Balance))
	{
		return false;
	}
	TestTrue(TEXT("shipped: LL3 locked at tier II"), Balance->IsAmmoLocked(StartechTags::Item_Ammo_LL3, 2, Reason));
	TestEqual(TEXT("shipped: reason"), Reason.ToString(), FString(TEXT("Reach Glacier (tier III)")));
	TestFalse(TEXT("shipped: LL3 unlocked at tier III"), Balance->IsAmmoLocked(StartechTags::Item_Ammo_LL3, 3, Reason));
	TestFalse(TEXT("shipped: LL2 sold from the start"), Balance->IsAmmoLocked(StartechTags::Item_Ammo_LL2, 1, Reason));
	TestTrue(TEXT("shipped: LL1 is the free ammo"), Balance->FreeAmmoTag.MatchesTagExact(StartechTags::Item_Ammo_LL1));
	TestNotNull(TEXT("shipped: LL1 in the item catalog"), Balance->FindFreeAmmo());

	// A shop with LL3 listed offers it, but the trade is locked until tier III (no profile here: tier I).
	UAmmoData* const LL3 = UStartechSettings::Get().FindItemOfType<UAmmoData>(StartechTags::Item_Ammo_LL3);
	if (TestNotNull(TEXT("LL3 in the item catalog"), LL3))
	{
		UWorld* const World = UWorld::CreateWorld(EWorldType::None, false, TEXT("StartechCombatLockWorld"));
		AActor* const Trader = World->SpawnActor<AActor>();
		FShopEntry Entry;
		Entry.Good = LL3;
		TestTrue(TEXT("LL3 has a row"), FShopTrading::IsBuyOffered(Entry));
		TestTrue(TEXT("LL3 locked for a tier I trader"), FShopTrading::IsLocked(Entry, *Trader, Reason));
		World->DestroyWorld(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechCombatDropOnlyTest, "Startech.Combat.DropOnlyNeverSold",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechCombatDropOnlyTest::RunTest(const FString& Parameters)
{
	const UCombatBalance* const Balance = UCombatBalance::Get();
	if (!TestNotNull(TEXT("DA_CombatBalance set in UStartechSettings"), Balance))
	{
		return false;
	}
	TestTrue(TEXT("LL4 is drop-only"), Balance->DropOnlyAmmoTags.Contains(StartechTags::Item_Ammo_LL4));

	// No shop asset lists LL4 or LL1, and even if one did, it would get no Buy.
	const TArray<UShopData*> Shops = LoadAllShops();
	TestTrue(TEXT("found the shops"), Shops.Num() > 0);
	for (const UShopData* const Shop : Shops)
	{
		for (const FShopEntry& Entry : Shop->Entries)
		{
			const FString Where = FString::Printf(TEXT("%s: %s"), *Shop->GetName(), *GetNameSafe(Entry.Good));
			TestFalse(Where + TEXT(" is drop-only ammo"), Balance->IsDropOnly(Entry.Good));
			TestFalse(Where + TEXT(" is the free ammo"), Balance->IsFreeAmmo(Entry.Good));
		}
	}
	UAmmoData* const LL4 = UStartechSettings::Get().FindItemOfType<UAmmoData>(StartechTags::Item_Ammo_LL4);
	if (TestNotNull(TEXT("LL4 in the item catalog"), LL4))
	{
		FShopEntry Entry;
		Entry.Good = LL4;
		TestFalse(TEXT("LL4 never offered"), FShopTrading::IsBuyOffered(Entry));
	}

	// LL4 drops only from tier III and IV enemies.
	constexpr int32 Rolls = 4000;
	for (int32 Tier = 1; Tier <= 4; ++Tier)
	{
		int32 Drops = 0;
		for (int32 Roll = 0; Roll < Rolls; ++Roll)
		{
			FLootBundle Bundle;
			Balance->RollTierAmmoDrops(Tier, Bundle);
			for (const FAmmoStack& Stack : Bundle.Ammo)
			{
				if (Stack.Ammo == LL4)
				{
					++Drops;
					TestTrue(TEXT("LL4 amount 20-60"), Stack.Count >= 20.f && Stack.Count <= 60.f);
				}
			}
		}
		const float Rate = static_cast<float>(Drops) / Rolls;
		AddInfo(FString::Printf(TEXT("tier %d: LL4 dropped in %.1f%% of kills"), Tier, Rate * 100.f));
		if (Tier <= 2)
		{
			TestEqual(FString::Printf(TEXT("no LL4 at tier %d"), Tier), Drops, 0);
		}
		else
		{
			const float Expected = Tier == 3 ? 0.15f : 0.25f;
			TestTrue(FString::Printf(TEXT("tier %d LL4 rate near %.0f%%"), Tier, Expected * 100.f),
				FMath::Abs(Rate - Expected) < 0.03f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechCombatTTKTargetsTest, "Startech.Combat.TTKTargets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechCombatTTKTargetsTest::RunTest(const FString& Parameters)
{
	const UCombatBalance* const Balance = UCombatBalance::Get();
	if (!TestNotNull(TEXT("DA_CombatBalance set in UStartechSettings"), Balance))
	{
		return false;
	}

	const FCombatTTKTable Table = FCombatTTKTable::Build(*Balance);
	for (const FString& Line : Table.ToLines())
	{
		AddInfo(Line);
	}
	for (const FString& Error : Table.Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("the table has rows"), Table.Rows.Num() > 0);

	// Every regular enemy inside its time-to-kill target with the expected gear; and nothing else marked either.
	for (const FCombatTTKRow& Row : Table.Rows)
	{
		const FString Where = FString::Printf(TEXT("tier %d %s with %s"), Row.Tier, *Row.Enemy, *Row.Gear);
		if (Row.bTimeToKillOutside)
		{
			AddError(FString::Printf(TEXT("%s: time to kill %.2f s outside %.2f-%.2f s"), *Where, Row.TimeToKill,
				Row.AllowedMinTimeToKill, Row.AllowedMaxTimeToKill));
		}
		if (Row.bLossOutside)
		{
			AddError(FString::Printf(TEXT("%s: one fight costs %.1f%% of health, outside %.0f-%.0f%%"), *Where,
				Row.LoseOne * 100.f, Row.AllowedMinLoss * 100.f, Row.AllowedMaxLoss * 100.f));
		}
		// Information only (the chaser's back-off value): printed, never failing.
		if (Row.bAltTargetInfoOnly && Row.LoseAlt >= 0.f)
		{
			const FString Aim = Row.AllowedMaxAltLoss > 0.f
				? FString::Printf(TEXT("aim %.0f-%.0f%%"), Row.AllowedMinAltLoss * 100.f, Row.AllowedMaxAltLoss * 100.f)
				: FString(TEXT("no target"));
			AddInfo(FString::Printf(TEXT("%s: %s costs %.1f%% of health (information; %s)"), *Where, *Row.AltCase,
				Row.LoseAlt * 100.f, *Aim));
		}
		if (Row.bAltLossOutside)
		{
			AddError(FString::Printf(TEXT("%s: %s costs %.1f%% of health, outside %.0f-%.0f%%"), *Where, *Row.AltCase,
				Row.LoseAlt * 100.f, Row.AllowedMinAltLoss * 100.f, Row.AllowedMaxAltLoss * 100.f));
		}
		if (Row.bCostOutside)
		{
			AddError(FString::Printf(TEXT("%s: ammo costs %.1f%% of the kill's value, over %.0f%%"), *Where,
				Row.CostShare * 100.0, Balance->MaxAmmoCostShare * 100.f));
		}
	}
	return true;
}

#endif
