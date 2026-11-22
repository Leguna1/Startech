#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuHomePanel.generated.h"

class UHubButtonWidget;
class UTextBlock;

/**
 * The main menu's first page: the title "Startech" and the buttons Continue (with the pilot it continues, hidden when
 * no slot is occupied), New game, Load (disabled when no slot is occupied) and Quit, in a column.
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

	/** Quit was clicked. */
	FSimpleMulticastDelegate OnQuit;

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

	/** Quit. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> QuitButton;
};
