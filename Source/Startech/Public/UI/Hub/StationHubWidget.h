#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubTab.h"
#include "UI/Hub/HubWidgetBase.h"
#include "StationHubWidget.generated.h"

class UHubQuestsTabWidget;
class UHubTabRailWidget;
class UHubTopBarWidget;
class UHubUpgradesTabWidget;
class UShopScreenWidget;
class UWidgetSwitcher;

/**
 * The station hub: a full-screen panel over the game while docked, opened and closed by the player ship
 * (AStartechShipPawn::OpenHub / CloseHub). The C++ parent of WBP_StationHub, which only places the panels: the top bar,
 * the tab rail, and a content switcher holding the Quests tab, the existing shop screen and the Upgrades tab (made here
 * when the Blueprint doesn't place one).
 * It opens on Quests when a quest is ready to turn in, otherwise on the last tab used (kept by the ship). Escape, or
 * the interact key that opened it, closes it; the player stays docked. Every panel uses this widget's Style.
 */
UCLASS(Abstract)
class STARTECH_API UStationHubWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse and keyboard focus. */
	UStationHubWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows Tab in the content switcher, highlights it in the rail and remembers it on the ship. */
	void SelectTab(EHubTab Tab);

protected:
	/** Binds the rail and picks the opening tab. */
	virtual void NativeConstruct() override;

	/** Unbinds the rail. */
	virtual void NativeDestruct() override;

	/** Escape or the interact key closes the hub; on the Shop tab, keys 1-5 pick the quantity. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** The full-width bar across the top. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHubTopBarWidget> TopBar;

	/** The tab rail on the left. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHubTabRailWidget> TabRail;

	/** Holds QuestsTab and ShopScreen; shows the selected one. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher;

	/** The Quests tab, a child of ContentSwitcher. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHubQuestsTabWidget> QuestsTab;

	/** The existing shop screen (WB_Shop_Screen), a child of ContentSwitcher. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShopScreenWidget> ShopScreen;

	/**
	 * The Upgrades tab, a child of ContentSwitcher. Optional in WBP_StationHub: without one, NativeConstruct makes it
	 * and adds it to ContentSwitcher.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHubUpgradesTabWidget> UpgradesTab;
};
