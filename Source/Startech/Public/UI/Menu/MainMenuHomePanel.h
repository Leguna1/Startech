#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuHomePanel.generated.h"

class UHubButtonWidget;
class UTextBlock;

/**
 * The main menu's first page: the title "Startech" and the buttons Continue (with the pilot it continues, hidden when
 * no slot is occupied), New game, Load (disabled when no slot is occupied), Settings, Controls and Quit, in a column.
 */
UCLASS()
class STARTECH_API UMainMenuHomePanel : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Shows Continue with Detail under it (for example "Ace · 1:24 played"), or hides it. */
	void SetContinue(bool bAvailable, const FText& Detail);

	/** Enables or disables Load. */
	void SetLoadEnabled(bool bEnabled);

	/** Continue was clicked. */
	FSimpleMulticastDelegate OnContinue;

	/** New game was clicked. */
	FSimpleMulticastDelegate OnNewGame;

	/** Load was clicked. */
	FSimpleMulticastDelegate OnLoad;

	/** Settings was clicked. */
	FSimpleMulticastDelegate OnSettings;

	/** Controls was clicked. */
	FSimpleMulticastDelegate OnControls;

	/** Quit was clicked. */
	FSimpleMulticastDelegate OnQuit;

	/** The Controls button, for tests. */
	UHubButtonWidget* GetControlsButton() const { return ControlsButton; }

	/** The Settings button, for tests. */
	UHubButtonWidget* GetSettingsButton() const { return SettingsButton; }

protected:
	/** The title and the button column, centred. */
	virtual void BuildContent() override;

	/** Forwards the buttons' clicks. */
	virtual void NativeConstruct() override;

	/** Unbinds the buttons. */
	virtual void NativeDestruct() override;

private:
	/** Adds a button labelled Label to the column. */
	UHubButtonWidget* AddButton(class UVerticalBox& Column, const FText& Label);

	/** Continue. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ContinueButton;

	/** What Continue continues. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinueDetail;

	/** New game. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> NewGameButton;

	/** Load. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> LoadButton;

	/** Settings. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> SettingsButton;

	/** Controls. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ControlsButton;

	/** Quit. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> QuitButton;
};
