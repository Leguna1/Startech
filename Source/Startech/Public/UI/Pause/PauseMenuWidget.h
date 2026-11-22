#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "PauseMenuWidget.generated.h"

class UMainMenuDialog;
class UPauseMenuPanel;

/**
 * The pause menu: the C++ parent of WBP_PauseMenu, which only places the panel and the dialog. Opened and closed by
 * UStartechPauseMenuSubsystem (the game is paused while it shows).
 * - Resume, or Escape: back to the game.
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

protected:
	/** Binds the panel. */
	virtual void NativeConstruct() override;

	/** Unbinds the panel. */
	virtual void NativeDestruct() override;

	/** Escape: closes the dialog, else resumes. */
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

	/** Quit to main menu, after the warning when not docked. */
	void HandleQuitToMainMenu();

	/** Quit game, after the warning when not docked. */
	void HandleQuitGame();

	/** Runs Quit at once when docked, else after the player confirms the warning with QuitLabel. */
	void ConfirmQuit(const FText& QuitLabel, TFunction<void()> Quit);
};
