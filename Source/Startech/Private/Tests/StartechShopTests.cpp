#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AmmoData.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "LaserWeaponData.h"
#include "ShopData.h"
#include "ShopEntryWidget.h"
#include "ShopTrading.h"
#include "Tests/ShopTestCreditsListener.h"
#include "WeaponSystemComponent.h"

namespace StartechShopTests
{
	/**
	 * A bare world (no game subsystems) holding one trader actor with an inventory and a weapon system, and a shop at
	 * markup 1.5 / buy rate 1.0. Destroys the world when it goes out of scope.
	 */
	struct FShopFixture
	{
		FShopFixture()
		{
			World = UWorld::CreateWorld(EWorldType::None, false, TEXT("StartechShopTestWorld"));
			Trader = World->SpawnActor<AActor>();

			Inventory = NewObject<UInventoryComponent>(Trader, TEXT("Inventory"));
			Inventory->RegisterComponent();
			Weapons = NewObject<UWeaponSystemComponent>(Trader, TEXT("WeaponSystem"));
			Weapons->RegisterComponent();

			Shop = NewObject<UShopData>(GetTransientPackage());
			Shop->SellMarkup = 1.5f;
			Shop->BuyRate = 1.f;

			Listener = NewObject<UShopTestCreditsListener>(GetTransientPackage());
			Inventory->OnCreditsChanged.AddDynamic(Listener, &UShopTestCreditsListener::HandleCreditsChanged);

			TradeHandle = FShopTrading::OnTradeCompleted.AddLambda([this](const FTradeOutcome&, AActor&) { ++Trades; });
		}

		~FShopFixture()
		{
			FShopTrading::OnTradeCompleted.Remove(TradeHandle);
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		/** Adds an entry for a new good of class T with BaseValue, sold and bought here; returns its index. */
		template <typename T>
		int32 AddGood(const float BaseValue, T*& OutGood)
		{
			OutGood = NewObject<T>(GetTransientPackage());
			OutGood->BaseValue = BaseValue;
			OutGood->DisplayName = FText::FromString(TEXT("Good"));
			FShopEntry& Entry = Shop->Entries.AddDefaulted_GetRef();
			Entry.Good = OutGood;
			return Shop->Entries.Num() - 1;
		}

		/** Sets the credits without counting it as a trade's credit change. */
		void SetCredits(const int32 Credits)
		{
			Inventory->RestoreCredits(Credits);
			Listener->Count = 0;
		}

		const FShopEntry& Entry(const int32 Index) const { return Shop->Entries[Index]; }

		UWorld* World = nullptr;
		AActor* Trader = nullptr;
		UInventoryComponent* Inventory = nullptr;
		UWeaponSystemComponent* Weapons = nullptr;
		UShopData* Shop = nullptr;
		UShopTestCreditsListener* Listener = nullptr;
		int32 Trades = 0;
		FDelegateHandle TradeHandle;
	};
}

using namespace StartechShopTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechShopBulkBuyTest, "Startech.Shop.BulkBuy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechShopBulkBuyTest::RunTest(const FString& Parameters)
{
	FShopFixture Fixture;
	UItemData* Ferrite = nullptr;
	const int32 Index = Fixture.AddGood(8.f, Ferrite);
	Fixture.Inventory->SetCargoCapacity(200);
	Fixture.SetCredits(5000);

	// ×100 at 12 cr each, with enough credits and room.
	const FTradeOutcome Outcome = FShopTrading::BuyItem(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 100);
	TestTrue(TEXT("bought"), Outcome.IsSuccess());
	TestEqual(TEXT("quantity"), Outcome.Quantity, 100);
	TestEqual(TEXT("total"), Outcome.Credits, static_cast<int64>(1200));
	TestEqual(TEXT("credits left"), Fixture.Inventory->GetCredits(), 3800);
	TestEqual(TEXT("ferrite held"), Fixture.Inventory->GetItemCount(Ferrite), 100);

	// One transaction: one credit change and one trade event (the autosave's single "Trade" request).
	TestEqual(TEXT("one OnCreditsChanged"), Fixture.Listener->Count, 1);
	TestEqual(TEXT("one OnTradeCompleted"), Fixture.Trades, 1);
	TestEqual(TEXT("label"), UShopEntryWidget::GetTradeLabel(Outcome).ToString(), FString(TEXT("Buy 100")));
	// The number's grouping follows the locale ("1,200" or "1 200").
	TestEqual(TEXT("accent"), UShopEntryWidget::GetTradeAccent(Outcome).ToString(),
		FText::AsNumber(1200).ToString() + TEXT(" cr"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechShopClampTest, "Startech.Shop.Clamp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechShopClampTest::RunTest(const FString& Parameters)
{
	FShopFixture Fixture;
	UItemData* Ferrite = nullptr;
	const int32 Index = Fixture.AddGood(8.f, Ferrite);
	Fixture.Inventory->SetCargoCapacity(1000);

	// By credits: 500 cr buys 41 at 12 cr (492).
	Fixture.SetCredits(500);
	FTradeOutcome Preview = FShopTrading::PreviewBuy(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 100);
	TestEqual(TEXT("credit clamp quantity"), Preview.Quantity, 41);
	TestEqual(TEXT("credit clamp total"), Preview.Credits, static_cast<int64>(492));
	TestEqual(TEXT("credit clamp label"), UShopEntryWidget::GetTradeLabel(Preview).ToString(), FString(TEXT("Buy 41")));
	TestEqual(TEXT("preview changes nothing"), Fixture.Inventory->GetCredits(), 500);

	// By cargo: 66 of 100 held leaves room for 34.
	Fixture.Inventory->SetCargoCapacity(100);
	Fixture.Inventory->AddItem(Ferrite, 66);
	Fixture.SetCredits(100000);
	Preview = FShopTrading::PreviewBuy(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 100);
	TestEqual(TEXT("cargo clamp quantity"), Preview.Quantity, 34);
	TestEqual(TEXT("cargo clamp label"), UShopEntryWidget::GetTradeLabel(Preview).ToString(), FString(TEXT("Buy 34")));

	const FTradeOutcome Bought = FShopTrading::BuyItem(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 100);
	TestEqual(TEXT("cargo clamp bought"), Bought.Quantity, 34);
	TestEqual(TEXT("hold full"), Fixture.Inventory->GetCargoFree(), 0);

	// Ammo takes no cargo: only credits limit it.
	UAmmoData* Ammo = nullptr;
	const int32 AmmoIndex = Fixture.AddGood(1.f, Ammo);
	Fixture.SetCredits(300);
	const FTradeOutcome AmmoBuy = FShopTrading::BuyItem(*Fixture.Shop, Fixture.Entry(AmmoIndex), *Fixture.Trader, 1000);
	TestEqual(TEXT("ammo clamp by credits"), AmmoBuy.Quantity, 200);
	TestEqual(TEXT("ammo added"), FMath::RoundToInt(Fixture.Weapons->GetAmmoCount(Ammo)), 200);

	// Weapons trade one at a time.
	ULaserWeaponData* Laser = nullptr;
	const int32 LaserIndex = Fixture.AddGood(10.f, Laser);
	Fixture.SetCredits(10000);
	TestEqual(TEXT("weapon one at a time"),
		FShopTrading::BuyItem(*Fixture.Shop, Fixture.Entry(LaserIndex), *Fixture.Trader, 10).Quantity, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechShopMaxTest, "Startech.Shop.Max",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechShopMaxTest::RunTest(const FString& Parameters)
{
	FShopFixture Fixture;
	UItemData* Ferrite = nullptr;
	const int32 Index = Fixture.AddGood(8.f, Ferrite);
	Fixture.Inventory->SetCargoCapacity(100);
	Fixture.Inventory->AddItem(Ferrite, 30);

	// Buying: the lower of affordable (1,000 / 12 = 83) and free cargo (70).
	Fixture.SetCredits(1000);
	TestEqual(TEXT("max buy by cargo"), FShopTrading::GetMaxBuyable(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader), 70);
	Fixture.SetCredits(600);
	TestEqual(TEXT("max buy by credits"), FShopTrading::GetMaxBuyable(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader), 50);

	// Selling: everything held.
	TestEqual(TEXT("max sell"), FShopTrading::GetMaxSellable(Fixture.Entry(Index), *Fixture.Trader), 30);
	const FTradeOutcome Sold = FShopTrading::SellItem(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader,
		FShopTrading::GetMaxSellable(Fixture.Entry(Index), *Fixture.Trader));
	TestEqual(TEXT("sold all"), Sold.Quantity, 30);
	TestEqual(TEXT("sold for"), Sold.Credits, static_cast<int64>(240));
	TestEqual(TEXT("none left"), Fixture.Inventory->GetItemCount(Ferrite), 0);
	TestEqual(TEXT("sell label"), UShopEntryWidget::GetTradeLabel(Sold).ToString(), FString(TEXT("Sell 30")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechShopRefuseTest, "Startech.Shop.Refuse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechShopRefuseTest::RunTest(const FString& Parameters)
{
	FShopFixture Fixture;
	UItemData* Ferrite = nullptr;
	const int32 Index = Fixture.AddGood(8.f, Ferrite);

	// Not even one is affordable: 5 cr against 12.
	Fixture.SetCredits(5);
	FTradeOutcome Outcome = FShopTrading::BuyItem(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 10);
	TestFalse(TEXT("refused"), Outcome.IsSuccess());
	TestEqual(TEXT("credits reason"), Outcome.Result, ETradeResult::NotEnoughCredits);
	TestEqual(TEXT("credits label"), UShopEntryWidget::GetTradeLabel(Outcome).ToString(), FString(TEXT("Need 7 more credits")));
	TestEqual(TEXT("nothing spent"), Fixture.Inventory->GetCredits(), 5);
	TestEqual(TEXT("no trade event"), Fixture.Trades, 0);

	// A full hold.
	Fixture.Inventory->SetCargoCapacity(10);
	Fixture.Inventory->AddItem(Ferrite, 10);
	Fixture.SetCredits(1000);
	Outcome = FShopTrading::PreviewBuy(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 1);
	TestEqual(TEXT("cargo reason"), Outcome.Result, ETradeResult::CargoFull);
	TestEqual(TEXT("cargo label"), UShopEntryWidget::GetTradeLabel(Outcome).ToString(), FString(TEXT("Cargo full")));

	// Nothing to sell.
	UItemData* Aurium = nullptr;
	const int32 AuriumIndex = Fixture.AddGood(75.f, Aurium);
	Outcome = FShopTrading::SellItem(*Fixture.Shop, Fixture.Entry(AuriumIndex), *Fixture.Trader, 1);
	TestEqual(TEXT("sell reason"), Outcome.Result, ETradeResult::NothingToSell);
	TestEqual(TEXT("sell label"), UShopEntryWidget::GetTradeLabel(Outcome).ToString(), FString(TEXT("None to sell")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechShopRoundingTest, "Startech.Shop.Rounding",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FStartechShopRoundingTest::RunTest(const FString& Parameters)
{
	// The rule: totals only, up when buying, down when selling.
	TestEqual(TEXT("buy 0.45 x 10 = 4.5 -> 5"), FShopTrading::GetBuyTotal(0.45, 10), static_cast<int64>(5));
	TestEqual(TEXT("buy 0.45 x 100 = 45"), FShopTrading::GetBuyTotal(0.45, 100), static_cast<int64>(45));
	TestEqual(TEXT("sell 0.3 x 7 = 2.1 -> 2"), FShopTrading::GetSellTotal(0.3, 7), static_cast<int64>(2));
	TestEqual(TEXT("sell 0.3 x 10 = 3"), FShopTrading::GetSellTotal(0.3, 10), static_cast<int64>(3));
	TestEqual(TEXT("buy 0.1 x 30 = 3, not 4"), FShopTrading::GetBuyTotal(0.1, 30), static_cast<int64>(3));

	// Through a shop: a mineral at base 0.3 (buy 0.45 each, sell 0.3 each).
	FShopFixture Fixture;
	UItemData* Dust = nullptr;
	const int32 Index = Fixture.AddGood(0.3f, Dust);
	Fixture.Inventory->SetCargoCapacity(1000);
	Fixture.SetCredits(100);

	const FTradeOutcome Bought = FShopTrading::BuyItem(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 10);
	TestEqual(TEXT("bought 10 for 5"), Bought.Credits, static_cast<int64>(5));
	TestEqual(TEXT("credits after buy"), Fixture.Inventory->GetCredits(), 95);

	const FTradeOutcome Sold = FShopTrading::SellItem(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader, 7);
	TestEqual(TEXT("sold 7 for 2"), Sold.Credits, static_cast<int64>(2));
	TestEqual(TEXT("credits after sell"), Fixture.Inventory->GetCredits(), 97);

	// The most affordable never overspends: 3 cr buys 6 at 0.45 (2.7 -> 3), not 7 (3.15 -> 4).
	Fixture.SetCredits(3);
	TestEqual(TEXT("affordable with rounding"), FShopTrading::GetMaxBuyable(*Fixture.Shop, Fixture.Entry(Index), *Fixture.Trader), 6);
	return true;
}

#endif
