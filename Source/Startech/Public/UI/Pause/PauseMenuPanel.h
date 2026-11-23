#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "PauseMenuPanel.generated.h"

class UHubButtonWidget;

/** The pause menu's panel: the title "Paused" and the buttons Resume, Settings, Controls, Quit to main menu and Quit game, in a column. */
UCLASS()
class STARTECH_API UPauseMenuPanel : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Resume was clicked. */
	FSimpleMulticastDelegate OnResume;

	/** Settings was clicked. */
	FSimpleMulticastDelegate OnSettings;

	/** Controls was clicked. */
	FSimpleMulticastDelegate OnControls;

	/** Quit to main menu was clicked. */
	FSimpleMulticastDelegate OnQuitToMainMenu;

	/** The Controls button, for tests. */
	UHubButtonWidget* GetControlsButton() const { return ControlsButton; }

	/** The Settings button, for tests. */
	UHubButtonWidget* GetSettingsButton() const { return SettingsButton; }

	/** Quit game was clicked. */
	FSimpleMulticastDelegate OnQuitGame;

protected:
	/** A cut-corner panel with the title and the button column. */
	virtual void BuildContent() override;

	/** Forwards the buttons' clicks. */
	virtual void NativeConstruct() override;

	/** Unbinds the buttons. */
	virtual void NativeDestruct() override;

private:
	/** Resume. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ResumeButton;

	/** Settings. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> SettingsButton;

	/** Controls. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ControlsButton;

	/** Quit to main menu. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> MainMenuButton;

	/** Quit game. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> QuitButton;
};
