#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "PauseMenuWidget.generated.h"

class UControlsScreenWidget;
class UMenuPageWidget;
class USettingsScreenWidget;
class UHubButtonWidget;
class UMainMenuDialog;
class UPauseMenuPanel;

/**
 * The pause menu: the C++ parent of WBP_PauseMenu, which only places the panel and the dialog. Opened and closed by
 * UStartechPauseMenuSubsystem (the game is paused while it shows).
 * - Resume, or Escape: back to the game.
 * - Controls: the Controls screen (UControlsScreenWidget, made in C++ over the panel); Back or Escape returns here.
 * - Settings: the Settings page (USettingsScreenWidget: volumes, no Menu music slider); Back or Escape returns here.
 * - Quit to main menu / Quit game: save (SaveNow "Quit"), close the slot, then leave (UStartechGameFlowSubsystem).
 *   When not docked, a warning comes first: "You'll resume docked at <home> station. Cargo in space and unfinished
 *   trips are lost. Your cargo hold, credits and quests are kept."
 * Every panel uses this widget's Style (DA_HudStyle).
 */
UCLASS(Abstract)
class STARTECH_API UPauseMenuWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse and keyboard focus. */
	UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Controls: hides the panel and opens the Controls screen. */
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

	/** The panel's Controls button, for tests. */
	UHubButtonWidget* GetControlsButton() const;

protected:
	/** Adds the Controls screen and the Settings page over the menu. */
	virtual void NativeOnInitialized() override;

	/** Binds the panel. */
	virtual void NativeConstruct() override;

	/** Unbinds the panel. */
	virtual void NativeDestruct() override;

	/** Escape: closes an open page (Controls, Settings) or the dialog, else resumes. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** The title and the buttons. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPauseMenuPanel> PausePanel;

	/** The confirmation box for quitting while not docked, over the panel. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMainMenuDialog> Dialog;

private:
	/** Resume. */
	void HandleResume();

	/** Hides the menu's own content and opens Page. */
	void OpenPage(UMenuPageWidget* Page);

	/** A page closed: shows the menu's own content again. */
	void HandlePageClosed();

	/** Quit to main menu, after the warning when not docked. */
	void HandleQuitToMainMenu();

	/** Quit game, after the warning when not docked. */
	void HandleQuitGame();

	/** Runs Quit at once when docked, else after the player confirms the warning with QuitLabel. */
	void ConfirmQuit(const FText& QuitLabel, TFunction<void()> Quit);

	/** Over the panel, collapsed until Controls is clicked. */
	UPROPERTY(Transient)
	TObjectPtr<UControlsScreenWidget> ControlsScreen;

	/** Over the menu, collapsed until Settings is clicked. */
	UPROPERTY(Transient)
	TObjectPtr<USettingsScreenWidget> SettingsScreen;

	/** PausePanel's visibility before a page hid it. */
	ESlateVisibility VisibilityBeforePage = ESlateVisibility::Visible;
};
