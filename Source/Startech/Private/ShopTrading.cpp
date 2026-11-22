#include "ShopTrading.h"

#include "AmmoData.h"
#include "Combat/CombatBalance.h"
#include "GameFramework/Actor.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "PlayerProfileSubsystem.h"
#include "ShopData.h"
#include "WeaponData.h"
#include "WeaponSystemComponent.h"

#define LOCTEXT_NAMESPACE "ShopTrading"

namespace ShopTrading
{
	/** Totals within this of a whole credit count as that credit, so 0.1 × 30 is 3, not 4 or 2. */
	constexpr double RoundingTolerance = 1e-6;
}

FOnTradeCompleted FShopTrading::OnTradeCompleted;

FTradeOutcome FShopTrading::BuyItem(const UShopData& Shop, const FShopEntry& Entry, AActor& Buyer, const int32 Quantity)
{
	FTradeOutcome Outcome = Buy(Shop, Entry, Buyer, Quantity, true);
	if (Outcome.IsSuccess())
	{
		OnTradeCompleted.Broadcast(Outcome, Buyer);
	}
	return Outcome;
}

FTradeOutcome FShopTrading::SellItem(const UShopData& Shop, const FShopEntry& Entry, AActor& Seller, const int32 Quantity)
{
	FTradeOutcome Outcome = Sell(Shop, Entry, Seller, Quantity, true);
	if (Outcome.IsSuccess())
	{
		OnTradeCompleted.Broadcast(Outcome, Seller);
	}
	return Outcome;
}

FTradeOutcome FShopTrading::PreviewBuy(const UShopData& Shop, const FShopEntry& Entry, const AActor& Buyer, const int32 Quantity)
{
	return Buy(Shop, Entry, Buyer, Quantity, false);
}

FTradeOutcome FShopTrading::PreviewSell(const UShopData& Shop, const FShopEntry& Entry, const AActor& Seller, const int32 Quantity)
{
	return Sell(Shop, Entry, Seller, Quantity, false);
}

int64 FShopTrading::GetBuyTotal(const double UnitPrice, const int32 Quantity)
{
	return static_cast<int64>(FMath::CeilToDouble(FMath::Max(UnitPrice, 0.0) * Quantity - ShopTrading::RoundingTolerance));
}

int64 FShopTrading::GetSellTotal(const double UnitPrice, const int32 Quantity)
{
	return static_cast<int64>(FMath::FloorToDouble(FMath::Max(UnitPrice, 0.0) * Quantity + ShopTrading::RoundingTolerance));
}

int32 FShopTrading::GetAffordableUnits(const double UnitPrice, const int64 Credits)
{
	if (UnitPrice <= 0.0)
	{
		return MAX_int32;
	}

	// Start from the exact division, then step down while the rounded-up total is still too much.
	int64 Units = static_cast<int64>(FMath::FloorToDouble(static_cast<double>(Credits) / UnitPrice + ShopTrading::RoundingTolerance));
	Units = FMath::Clamp<int64>(Units, 0, MAX_int32);
	while (Units > 0 && GetBuyTotal(UnitPrice, static_cast<int32>(Units)) > Credits)
	{
		--Units;
	}
	return static_cast<int32>(Units);
}

bool FShopTrading::IsBuyOffered(const FShopEntry& Entry)
{
	if (!Entry.bShopSells || !Entry.Good)
	{
		return false;
	}
	// Free ammo needs no buying, and drop-only ammo is never sold, whatever a shop asset lists.
	const UCombatBalance* const Balance = Entry.Good->IsA<UAmmoData>() ? UCombatBalance::Get() : nullptr;
	return !Balance || (!Balance->IsFreeAmmo(Entry.Good) && !Balance->IsDropOnly(Entry.Good));
}

bool FShopTrading::IsLocked(const FShopEntry& Entry, const AActor& Trader, FText& OutReason)
{
	const UCombatBalance* const Balance = Entry.Good && Entry.Good->IsA<UAmmoData>() ? UCombatBalance::Get() : nullptr;
	if (!Balance)
	{
		return false;
	}
	const UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(&Trader);
	return Balance->IsAmmoLocked(Entry.Good->ItemTag, Profile ? Profile->GetHighestTierVisited() : 1, OutReason);
}

bool FShopTrading::IsSellOffered(const FShopEntry& Entry)
{
	// Minerals and spare weapons are bought back; ammo never is.
	return Entry.bShopBuys && Entry.Good && (Entry.Good->IsA<UItemData>() || Entry.Good->IsA<UWeaponData>());
}

bool FShopTrading::IsSingleUnitGood(const FShopEntry& Entry)
{
	return Entry.Good && Entry.Good->IsA<UWeaponData>();
}

int32 FShopTrading::GetOwnedCount(const FShopEntry& Entry, const AActor& Trader)
{
	if (const UItemData* const Item = Cast<UItemData>(Entry.Good))
	{
		const UInventoryComponent* const Inventory = Trader.FindComponentByClass<UInventoryComponent>();
		return Inventory ? Inventory->GetItemCount(Item) : 0;
	}

	if (const UAmmoData* const Ammo = Cast<UAmmoData>(Entry.Good))
	{
		const UWeaponSystemComponent* const Weapons = Trader.FindComponentByClass<UWeaponSystemComponent>();
		return Weapons ? FMath::FloorToInt(Weapons->GetAmmoCount(Ammo)) : 0;
	}

	if (const UWeaponData* const Weapon = Cast<UWeaponData>(Entry.Good))
	{
		const UInventoryComponent* const Inventory = Trader.FindComponentByClass<UInventoryComponent>();
		return Inventory ? Inventory->GetWeaponCount(Weapon) : 0;
	}

	return 0;
}

int32 FShopTrading::GetMaxBuyable(const UShopData& Shop, const FShopEntry& Entry, const AActor& Buyer)
{
	const UInventoryComponent* const Inventory = Buyer.FindComponentByClass<UInventoryComponent>();
	FText LockReason;
	if (!IsBuyOffered(Entry) || !Inventory || IsLocked(Entry, Buyer, LockReason))
	{
		return 0;
	}

	int32 Units = GetAffordableUnits(Shop.GetPricePlayerPays(*Entry.Good), Inventory->GetCredits());
	if (Entry.Good->IsA<UItemData>())
	{
		Units = FMath::Min(Units, Inventory->GetCargoFree());
	}
	if (IsSingleUnitGood(Entry))
	{
		Units = FMath::Min(Units, 1);
	}
	return Units;
}

int32 FShopTrading::GetMaxSellable(const FShopEntry& Entry, const AActor& Seller)
{
	if (!IsSellOffered(Entry))
	{
		return 0;
	}
	const int32 Owned = GetOwnedCount(Entry, Seller);
	return IsSingleUnitGood(Entry) ? FMath::Min(Owned, 1) : Owned;
}

FTradeOutcome FShopTrading::Buy(const UShopData& Shop, const FShopEntry& Entry, const AActor& Buyer, const int32 Quantity,
	const bool bApply)
{
	FTradeOutcome Outcome;
	Outcome.Direction = ETradeDirection::Buy;
	Outcome.Good = Entry.Good;
	Outcome.Requested = FMath::Max(Quantity, 0);

	UInventoryComponent* const Inventory = Buyer.FindComponentByClass<UInventoryComponent>();
	if (!IsBuyOffered(Entry) || !Inventory || Quantity <= 0)
	{
		Outcome.Result = ETradeResult::NotTraded;
		return Outcome;
	}

	// The lock comes first: credits don't help before the tier is reached.
	if (IsLocked(Entry, Buyer, Outcome.Reason))
	{
		Outcome.Result = ETradeResult::Locked;
		return Outcome;
	}

	const double UnitPrice = Shop.GetPricePlayerPays(*Entry.Good);
	const int32 Wanted = IsSingleUnitGood(Entry) ? 1 : Quantity;
	const int32 Affordable = GetAffordableUnits(UnitPrice, Inventory->GetCredits());
	const bool bTakesCargo = Entry.Good->IsA<UItemData>();
	const int32 Fits = bTakesCargo ? Inventory->GetCargoFree() : MAX_int32;
	Outcome.Quantity = FMath::Min3(Wanted, Affordable, Fits);

	if (Outcome.Quantity <= 0)
	{
		// Room comes first: no amount of credits helps a full hold.
		if (Fits <= 0)
		{
			Outcome.Result = ETradeResult::CargoFull;
			Outcome.Reason = LOCTEXT("CargoFull", "Cargo full");
		}
		else
		{
			Outcome.Result = ETradeResult::NotEnoughCredits;
			Outcome.Reason = FText::Format(LOCTEXT("NeedCredits", "Need {0} more credits"),
				FText::AsNumber(GetBuyTotal(UnitPrice, 1) - Inventory->GetCredits()));
		}
		Outcome.Quantity = 0;
		return Outcome;
	}

	Outcome.Result = ETradeResult::Success;
	Outcome.Credits = GetBuyTotal(UnitPrice, Outcome.Quantity);
	if (!bApply)
	{
		return Outcome;
	}

	// One credit change and one cargo, ammo or weapon change, however many units.
	Inventory->SpendCredits(static_cast<int32>(Outcome.Credits));
	if (UItemData* const Item = Cast<UItemData>(Entry.Good))
	{
		// A purchase isn't a pickup: the shop's status line reports it, not the HUD pickup feed.
		Inventory->AddItem(Item, Outcome.Quantity, false);
	}
	else if (UAmmoData* const Ammo = Cast<UAmmoData>(Entry.Good))
	{
		if (UWeaponSystemComponent* const Weapons = Buyer.FindComponentByClass<UWeaponSystemComponent>())
		{
			Weapons->AddAmmo(Ammo, static_cast<float>(Outcome.Quantity));
		}
	}
	else if (UWeaponData* const Weapon = Cast<UWeaponData>(Entry.Good))
	{
		// A bought weapon becomes a spare; spares take no cargo space.
		Inventory->AddWeapon(Weapon, Outcome.Quantity);
	}
	return Outcome;
}

FTradeOutcome FShopTrading::Sell(const UShopData& Shop, const FShopEntry& Entry, const AActor& Seller, const int32 Quantity,
	const bool bApply)
{
	FTradeOutcome Outcome;
	Outcome.Direction = ETradeDirection::Sell;
	Outcome.Good = Entry.Good;
	Outcome.Requested = FMath::Max(Quantity, 0);

	UInventoryComponent* const Inventory = Seller.FindComponentByClass<UInventoryComponent>();
	if (!IsSellOffered(Entry) || !Inventory || Quantity <= 0)
	{
		Outcome.Result = ETradeResult::NotTraded;
		return Outcome;
	}

	// Asking to sell more than is held sells what is held. For weapons only spares count, so a fitted weapon can't
	// be sold.
	Outcome.Quantity = FMath::Min(Quantity, GetMaxSellable(Entry, Seller));
	if (Outcome.Quantity <= 0)
	{
		Outcome.Quantity = 0;
		Outcome.Result = ETradeResult::NothingToSell;
		Outcome.Reason = LOCTEXT("NoneToSell", "None to sell");
		return Outcome;
	}

	Outcome.Result = ETradeResult::Success;
	Outcome.Credits = GetSellTotal(Shop.GetPricePlayerReceives(*Entry.Good), Outcome.Quantity);
	if (!bApply)
	{
		return Outcome;
	}

	if (const UItemData* const Item = Cast<UItemData>(Entry.Good))
	{
		Inventory->RemoveItem(Item, Outcome.Quantity);
	}
	else if (const UWeaponData* const Weapon = Cast<UWeaponData>(Entry.Good))
	{
		Inventory->RemoveWeapon(Weapon, Outcome.Quantity);
	}
	Inventory->AddCredits(static_cast<int32>(FMath::Min<int64>(Outcome.Credits, MAX_int32)));
	return Outcome;
}

#undef LOCTEXT_NAMESPACE
