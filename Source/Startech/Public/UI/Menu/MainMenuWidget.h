#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuWidget.generated.h"

class UControlsScreenWidget;
class UMenuPageWidget;
class USettingsScreenWidget;
class UHubButtonWidget;
class UMainMenuCallsignPanel;
class UMainMenuDialog;
class UMainMenuHomePanel;
class UMainMenuSlotsPanel;
class UStartechGameFlowSubsystem;
class UWidgetSwitcher;

/**
 * The main menu screen: the C++ parent of WBP_MainMenu, which only places the panels (a page switcher holding the
 * home page, the slot page and the callsign page, with the dialog over it). It moves between the pages and hands every
 * action to UStartechGameFlowSubsystem:
 * - Continue: loads the most recently saved slot (hidden when no slot is occupied).
 * - New game: the slot page; an occupied slot asks "Overwrite <name>? This can't be undone." first; then the
 *   callsign page, whose Start creates the game.
 * - Load: the slot page with occupied slots only, each with Load and a quiet Delete (confirmed).
 * - Controls: the Controls screen (UControlsScreenWidget, made in C++ over the pages); Back returns home.
 * - Settings: the Settings page (USettingsScreenWidget: volumes, with the Menu music slider); Back returns home.
 * - Quit: closes the game.
 * Escape closes an open page (Controls, Settings) or the dialog, or goes back one page. Every panel uses this widget's Style (DA_HudStyle).
 */
UCLASS(Abstract)
class STARTECH_API UMainMenuWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse and keyboard focus. */
	UMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Home: Controls. Hides the pages and opens the Controls screen. */
	void OpenControls();

	/** The Controls screen, or null if it couldn't be made. */
	UControlsScreenWidget* GetControlsScreen() const { return ControlsScreen; }

	/** Settings: hides the menu's own content and opens the Settings page. */
	void OpenSettings();

	/** The Settings page, or null if it couldn't be made. */
	USettingsScreenWidget* GetSettingsScreen() const { return SettingsScreen; }

	/** The Settings button, for tests. */
	UHubButtonWidget* GetSettingsButton() const;

	/** The open page (Controls or Settings), or null. */
	UMenuPageWidget* GetOpenPage() const;

	/** The home page's Controls button, for tests. */
	UHubButtonWidget* GetControlsButton() const;

protected:
	/** Adds the Controls screen and the Settings page over the menu. */
	virtual void NativeOnInitialized() override;

	/** Binds the panels and shows the home page. */
	virtual void NativeConstruct() override;

	/** Unbinds the panels. */
	virtual void NativeDestruct() override;

	/** Escape: closes an open page or the dialog, or goes back one page. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Holds HomePanel, SlotsPanel and CallsignPanel; shows one. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> PageSwitcher;

	/** The first page, a child of PageSwitcher. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMainMenuHomePanel> HomePanel;

	/** The slot page, a child of PageSwitcher. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMainMenuSlotsPanel> SlotsPanel;

	/** The callsign page, a child of PageSwitcher. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMainMenuCallsignPanel> CallsignPanel;

	/** The confirmation box, over the pages. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMainMenuDialog> Dialog;

private:
	/** Hides the menu's own content and opens Page. */
	void OpenPage(UMenuPageWidget* Page);

	/** A page closed: shows the menu's own content again. */
	void HandlePageClosed();

	/** Shows the home page with Continue and Load set from the slots on disk. */
	void ShowHome();

	/** Shows the slot page in Mode. */
	void ShowSlots(bool bLoadMode);

	/** Home: Continue. */
	void HandleContinue();

	/** Slot page: Select (new game) or Load on SlotIndex. */
	void HandleSlotPicked(int32 SlotIndex);

	/** Slot page: Delete on SlotIndex (asks first). */
	void HandleDeleteRequested(int32 SlotIndex);

	/** Callsign page: Start. */
	void HandleStart(const FString& Callsign);

	/** Shows the callsign page for SlotIndex. */
	void ShowCallsign(int32 SlotIndex);

	/** The display name of SlotIndex, or "Slot N". */
	FText GetSlotName(int32 SlotIndex) const;

	/** The game flow, or null in the designer. */
	UStartechGameFlowSubsystem* GetFlow() const;

	/** True while the slot page lists slots for loading. */
	bool bSlotsLoadMode = false;

	/** The slot the callsign page creates the game in. */
	int32 PendingNewGameSlot = INDEX_NONE;

	/** Over the pages, collapsed until Controls is clicked. */
	UPROPERTY(Transient)
	TObjectPtr<UControlsScreenWidget> ControlsScreen;

	/** Over the menu, collapsed until Settings is clicked. */
	UPROPERTY(Transient)
	TObjectPtr<USettingsScreenWidget> SettingsScreen;

	/** PageSwitcher's visibility before a page hid it. */
	ESlateVisibility VisibilityBeforePage = ESlateVisibility::Visible;
};
