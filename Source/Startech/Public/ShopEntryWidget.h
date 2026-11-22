#pragma once

#include "CoreMinimal.h"
#include "ShopQuantity.h"
#include "ShopTrading.h"
#include "UI/Hub/HubWidgetBase.h"
#include "ShopEntryWidget.generated.h"

class AActor;
class UHubButtonWidget;
class UShopData;
class UTradeableData;
struct FShopEntry;

/** Native: a row's Buy or Sell button made (or failed) a trade. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShopRowTraded, const FTradeOutcome&);

/**
 * One row of a shop, built in C++ like the hub's other rows (UShopScreenWidget::EntryWidgetClass points at this class):
 * the good's colour chip, name and one-line detail, how many you hold, then a Buy button (Primary) and a Sell button
 * (Secondary), both UHubButtonWidget. A side the shop doesn't offer is hidden.
 * Each button trades the screen's quantity (SetQuantity), or Max when Shift is held at the click, as one FShopTrading
 * call. Its label is the preview at that quantity, clamped to what works: "Buy 34 · 408 cr", with "12 cr each" under
 * it; when not even one unit works it is disabled and its label is the reason ("Need 12 more credits", "Cargo full").
 * Weapons always trade one at a time. UShopScreenWidget calls Refresh on every credit, cargo, ammo and weapon change.
 */
UCLASS()
class STARTECH_API UShopEntryWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Draws, and takes the mouse for its buttons. */
	UShopEntryWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows entry InEntryIndex of InShop. */
	void Setup(UShopData* InShop, int32 InEntryIndex);

	/** Re-reads the amount held, and the Buy and Sell previews at the current quantity. */
	void Refresh();

	/** The quantity the buttons trade, from the screen's chips. Refreshes. */
	void SetQuantity(EShopQuantity InQuantity);

	/** What a click would do now: Direction at InQuantity (Max: as many as possible). */
	FTradeOutcome Preview(ETradeDirection Direction, EShopQuantity InQuantity) const;

	/** What a click does: trades at the current quantity, or Max when bForceMax (Shift held), and broadcasts OnTraded. */
	FTradeOutcome Trade(ETradeDirection Direction, bool bForceMax);

	/** The Buy or Sell button, for test commands that click it through Slate. */
	UHubButtonWidget* GetButton(ETradeDirection Direction) const { return Direction == ETradeDirection::Buy ? BuyAction : SellAction; }

	/** The good shown, or null. */
	const UTradeableData* GetGood() const;

	/** A trade button's label for Outcome: "Buy 34" (the amount that works), or the reason when nothing does. */
	static FText GetTradeLabel(const FTradeOutcome& Outcome);

	/** The text after the label: the total, "408 cr"; empty when nothing can be traded. */
	static FText GetTradeAccent(const FTradeOutcome& Outcome);

	/** Fires after each click's trade, successful or not. */
	FOnShopRowTraded OnTraded;

protected:
	/** A fixed-height overlay with the Buy and Sell buttons on the right. */
	virtual void BuildContent() override;

	/** Binds the buttons. */
	virtual void NativeConstruct() override;

	/** Unbinds the buttons. */
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** Buy button: buys one purchase. */
	void HandleBuyClicked();

	/** Sell button: sells one unit. */
	void HandleSellClicked();

	/** Units a click asks for: the fixed quantity, everything possible for Max (at least 1), 1 for weapons. */
	int32 GetRequestedUnits(ETradeDirection Direction, EShopQuantity InQuantity) const;

	/** Sets Button's label, total and enabled state from Outcome, and "N cr each" from UnitPrice. */
	void ShowPreview(UHubButtonWidget& Button, const FTradeOutcome& Outcome, double UnitPrice) const;

	/** The entry shown, or null. */
	const FShopEntry* GetEntry() const;

	/** The player's ship. */
	AActor* GetTrader() const;

	/** A one-line summary of the good: a weapon's stats or an ammo's damage multiplier; empty for minerals. */
	static FText GetDetail(const UTradeableData& Good);

	/** The shop. */
	UPROPERTY(Transient)
	TObjectPtr<UShopData> Shop;

	/** The entry's index in Shop. */
	int32 EntryIndex = INDEX_NONE;

	/** The quantity the buttons trade. */
	EShopQuantity Quantity = EShopQuantity::One;

	/** The good's name. */
	FString Name;

	/** GetDetail, as text. */
	FString Detail;

	/** "Held 55". */
	FString Held;

	/** The good's colour, when it has one. */
	FLinearColor ChipColor = FLinearColor::Transparent;

	/** True when ChipColor is drawn. */
	bool bHasChip = false;

	/** Buys. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> BuyAction;

	/** Sells. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> SellAction;
};
