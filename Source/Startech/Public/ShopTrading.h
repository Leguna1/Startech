#pragma once

#include "CoreMinimal.h"
#include "ShopTrading.generated.h"

class AActor;
class UShopData;
class UTradeableData;
struct FShopEntry;

UENUM(BlueprintType)
enum class ETradeResult : uint8
{
	Success,
	NotEnoughCredits,
	CargoFull,
	NothingToSell,
	/** Not sold yet: the player hasn't reached the tier its unlock needs (UCombatBalance::AmmoUnlocks). */
	Locked,
	NotTraded
};

/** Which way a trade goes. */
UENUM(BlueprintType)
enum class ETradeDirection : uint8
{
	/** The player buys from the shop. */
	Buy,
	/** The player sells to the shop. */
	Sell
};

/**
 * What a trade did, or would do (FShopTrading::Preview*): how many units changed hands for how many credits. When
 * nothing could be traded, Result says why and Reason is the text for a disabled button ("Need 12 more credits",
 * "Cargo full", "None to sell").
 */
USTRUCT(BlueprintType)
struct STARTECH_API FTradeOutcome
{
	GENERATED_BODY()

	/** Success when Quantity > 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	ETradeResult Result = ETradeResult::NotTraded;

	/** Buy or sell. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	ETradeDirection Direction = ETradeDirection::Buy;

	/** Units traded: the requested amount, or less when credits, cargo space or the amount held ran out. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	int32 Quantity = 0;

	/** The amount asked for. Quantity < Requested means the trade was clamped. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	int32 Requested = 0;

	/** Credits paid (buying) or received (selling) for Quantity units. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	int64 Credits = 0;

	/** Why nothing could be traded; empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	FText Reason;

	/** The good traded. */
	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	TObjectPtr<const UTradeableData> Good = nullptr;

	/** True when at least one unit was (or would be) traded. */
	bool IsSuccess() const { return Result == ETradeResult::Success && Quantity > 0; }
};

/** Native: a trade was made (BuyItem or SellItem with Quantity > 0), by Trader. Autosave and tests listen. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTradeCompleted, const FTradeOutcome& /* Outcome */, AActor& /* Trader */);

/**
 * Every trade between a player and a shop goes through here. The trader's credits and cargo come from its
 * UInventoryComponent and its ammo from its UWeaponSystemComponent.
 *
 * Prices: a unit price may be fractional (UTradeableData::BaseValue × the shop's SellMarkup or BuyRate). Only totals
 * are rounded: up when buying, down when selling, so rounding never profits the player.
 * Quantities: a buy or sell of N units is one transaction (one credit change, one cargo or ammo change, one
 * OnTradeCompleted), clamped to the largest amount that works. Weapons always trade one at a time.
 */
class STARTECH_API FShopTrading
{
public:
	/** Buys up to Quantity units (clamped to credits and free cargo) as one transaction. */
	static FTradeOutcome BuyItem(const UShopData& Shop, const FShopEntry& Entry, AActor& Buyer, int32 Quantity);

	/** Sells up to Quantity units (clamped to the amount held) as one transaction. */
	static FTradeOutcome SellItem(const UShopData& Shop, const FShopEntry& Entry, AActor& Seller, int32 Quantity);

	/** What BuyItem would do, without changing anything: for the Buy button's label. */
	static FTradeOutcome PreviewBuy(const UShopData& Shop, const FShopEntry& Entry, const AActor& Buyer, int32 Quantity);

	/** What SellItem would do, without changing anything: for the Sell button's label. */
	static FTradeOutcome PreviewSell(const UShopData& Shop, const FShopEntry& Entry, const AActor& Seller, int32 Quantity);

	/** The most units Buyer can buy now: the lower of what is affordable and, for cargo goods, what fits. 1 for weapons. */
	static int32 GetMaxBuyable(const UShopData& Shop, const FShopEntry& Entry, const AActor& Buyer);

	/** The most units Seller can sell now: everything held (spares only for weapons, at most 1). */
	static int32 GetMaxSellable(const FShopEntry& Entry, const AActor& Seller);

	/** Credits for Quantity units at UnitPrice when buying: rounded up. */
	static int64 GetBuyTotal(double UnitPrice, int32 Quantity);

	/** Credits for Quantity units at UnitPrice when selling: rounded down. */
	static int64 GetSellTotal(double UnitPrice, int32 Quantity);

	/**
	 * What the row shows: whether each side of the entry is offered at all. Ammo is never bought back, and the free
	 * ammo (LL1) and drop-only ammo (LL4) are never sold (UCombatBalance), so they get no row.
	 */
	static bool IsBuyOffered(const FShopEntry& Entry);
	static bool IsSellOffered(const FShopEntry& Entry);

	/**
	 * True when the shop may not sell the entry's good to Trader yet; OutReason is the button label ("Reach Glacier
	 * (tier III)"). Uses the highest tier the player has reached (UPlayerProfileSubsystem).
	 */
	static bool IsLocked(const FShopEntry& Entry, const AActor& Trader, FText& OutReason);

	/** True for goods that trade one at a time (weapons). */
	static bool IsSingleUnitGood(const FShopEntry& Entry);

	/** How many of the entry's good the trader holds: cargo for minerals, rounds for ammo, spares for weapons. */
	static int32 GetOwnedCount(const FShopEntry& Entry, const AActor& Trader);

	/** Fires after every trade that moved at least one unit. */
	static FOnTradeCompleted OnTradeCompleted;

private:
	/** Buy: preview (bApply false) or apply. */
	static FTradeOutcome Buy(const UShopData& Shop, const FShopEntry& Entry, const AActor& Buyer, int32 Quantity, bool bApply);

	/** Sell: preview (bApply false) or apply. */
	static FTradeOutcome Sell(const UShopData& Shop, const FShopEntry& Entry, const AActor& Seller, int32 Quantity, bool bApply);

	/** The most units whose rounded-up total fits in Credits. */
	static int32 GetAffordableUnits(double UnitPrice, int64 Credits);
};
