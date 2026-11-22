#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuDialog.generated.h"

class UHubButtonWidget;
class UTextBlock;

/**
 * The main menu's confirmation box, drawn over a dimmed screen: a message, a confirm button and Cancel (for example
 * "Overwrite Ace? This can't be undone." with Overwrite and Cancel). Also shows plain messages with a single OK.
 * Collapsed until shown; placed by WBP_MainMenu over the pages.
 */
UCLASS()
class STARTECH_API UMainMenuDialog : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Asks Message; OnConfirm runs if ConfirmLabel's button is pressed. Cancel just closes. */
	void Ask(const FText& Message, const FText& ConfirmLabel, TFunction<void()> OnConfirm);

	/** Shows Message with a single OK that closes the box. */
	void ShowMessage(const FText& Message);

	/** Closes the box without confirming. */
	void Cancel();

	/** True while the box is shown. */
	bool IsOpen() const;

protected:
	/** A dimmed backdrop and a centred panel with the message and the buttons. */
	virtual void BuildContent() override;

	/** Binds the buttons and starts collapsed. */
	virtual void NativeConstruct() override;

	/** Unbinds the buttons. */
	virtual void NativeDestruct() override;

private:
	/** Confirm button callback: closes, then runs the pending action. */
	void HandleConfirmClicked();

	/** Shows the box with the given texts. */
	void Open(const FText& Message, const FText& ConfirmLabel, bool bShowCancel);

	/** The question or message. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	/** Confirms (or OK). */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ConfirmButton;

	/** Closes without confirming. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> CancelButton;

	/** What Confirm does; empty for a plain message. */
	TFunction<void()> PendingAction;
};
