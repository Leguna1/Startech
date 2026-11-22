#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopQuantity.h"
#include "ShopScreenWidget.generated.h"

class AStartechShipPawn;
class AStation;
class UAmmoData;
class UInventoryComponent;
class UMountRowWidget;
class UPanelWidget;
class UProgressBar;
class UShopEntryWidget;
class UShopQuantityBarWidget;
class UShopTabWidget;
struct FTradeOutcome;
class UTextBlock;
class UWeaponSystemComponent;

// The docked station's shops: one tab per shop plus a Loadout tab, and the selected tab's rows.
// Lay out the frame in a Widget Blueprint derived from this (WB_Shop_Screen). The tabs and rows are C++-built hub
// widgets (UShopTabWidget, UShopEntryWidget, UMountRowWidget, all on UHubButtonWidget), set in TabWidgetClass,
// EntryWidgetClass and MountRowWidgetClass by Tools/Shop/setup_shop.py. It is the station hub's Shop tab, so the
// hub's top bar shows credits and cargo and the hub closes it; its own credits and cargo readouts are optional.
UCLASS(Abstract)
class STARTECH_API UShopScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Indices 0 to (shop count - 1) are the station's shops; the shop count is the Loadout tab.
	void SelectShop(int32 TabIndex);

	/** Sets the quantity every Buy and Sell button trades, and its chip. ×1 when the hub opens. */
	void SetQuantity(EShopQuantity Quantity);

	/** The selected quantity. */
	EShopQuantity GetQuantity() const { return Quantity; }

	/**
	 * Keys 1-5 (number row or keypad) pick ×1, ×10, ×100, ×1,000 and Max while the chips are shown. True if Key was
	 * one of them. Called by UStationHubWidget while the Shop tab is open.
	 */
	bool HandleShortcutKey(const FKey& Key);

	/** The selected shop's rows, for test commands. */
	const TArray<TObjectPtr<UShopEntryWidget>>& GetRows() const { return Rows; }

	/** The status line's text, for test commands. */
	FText GetStatus() const { return StatusText; }

protected:
	/** Inserts the quantity chips and status line into the frame, before Slate builds it. */
	virtual void NativeOnInitialized() override;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Optional: the hub's top bar shows credits.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CreditsText;

	// Optional: the hub's top bar shows cargo.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CargoText;

	// Receives one tab per shop and the Loadout tab, e.g. a Horizontal Box.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> TabContainer;

	// Receives the selected shop's rows, or one mount row per weapon mount on the Loadout tab.
	// E.g. a Scroll Box or Vertical Box.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryContainer;

	// Optional: the selected shop's name, or "Loadout".
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShopNameText;

	// Optional: cargo used / capacity.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CargoBar;

	/**
	 * Optional: column headers placed in the Blueprint for the old hand-laid rows. The C++ rows (UShopEntryWidget) label
	 * their own columns, so this is collapsed.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ColumnHeaders;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UShopTabWidget> TabWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UShopEntryWidget> EntryWidgetClass;

	// One row per weapon mount on the Loadout tab.
	UPROPERTY(EditDefaultsOnly, Category = "Shop")
	TSubclassOf<UMountRowWidget> MountRowWidgetClass;

private:
	/** Puts the quantity chips and status line under TabContainer (made here; the Blueprint frame has no slot for them). */
	void BuildQuantityBar();

	/** The chips were clicked. */
	void HandleQuantityChanged(EShopQuantity NewQuantity);

	/** A row traded: reports it in the status line. */
	void HandleRowTraded(const FTradeOutcome& Outcome);

	/** True when the selected tab has bulk quantities: a shop with any good that isn't a weapon. */
	bool UsesQuantities() const;

	UFUNCTION()
	void HandleCreditsChanged(int32 NewCredits, int32 Delta);

	UFUNCTION()
	void HandleItemsChanged();

	UFUNCTION()
	void HandleWeaponsChanged();

	UFUNCTION()
	void HandleAmmoChanged(UAmmoData* Ammo, float NewCount);

	AStartechShipPawn* GetShip() const;

	int32 GetShopCount() const;
	bool IsLoadoutSelected() const;

	void BuildTabs();
	void BuildEntries();
	void BuildShopRows();
	void BuildMountRows();
	void Refresh();

	TWeakObjectPtr<AStation> Station;
	TWeakObjectPtr<UInventoryComponent> BoundInventory;
	TWeakObjectPtr<UWeaponSystemComponent> BoundWeapons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShopTabWidget>> Tabs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShopEntryWidget>> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMountRowWidget>> MountRows;

	// A shop index, the shop count for the Loadout tab, or INDEX_NONE.
	int32 SelectedTab = INDEX_NONE;

	/** The quantity every button trades. */
	EShopQuantity Quantity = EShopQuantity::One;

	/** The chips and the status line. */
	UPROPERTY(Transient)
	TObjectPtr<UShopQuantityBarWidget> QuantityBar;

	/** The status line's current text. */
	FText StatusText;
};
