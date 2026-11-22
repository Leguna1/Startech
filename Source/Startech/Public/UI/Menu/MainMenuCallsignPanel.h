#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuCallsignPanel.generated.h"

class UEditableTextBox;
class UHubButtonWidget;
class UTextBlock;

/** Broadcast with the trimmed, valid callsign. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCallsignEntered, const FString& /* Callsign */);

/**
 * The new-game page after a slot is chosen: a callsign field, the inline error "Use 3–16 letters, digits or spaces"
 * while the trimmed text breaks FCallsignRules, Start (enabled only when valid; Enter does the same) and Back.
 */
UCLASS()
class STARTECH_API UMainMenuCallsignPanel : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Clears the field for SlotIndex ("Slot 2") and focuses it. */
	void Prepare(int32 SlotIndex);

	/** Start (or Enter) with a valid callsign. */
	FOnCallsignEntered OnStart;

	/** Back was clicked. */
	FSimpleMulticastDelegate OnBack;

protected:
	/** The heading, the field, the error line and the buttons, in a fixed-width column. */
	virtual void BuildContent() override;

	/** Binds the field and the buttons. */
	virtual void NativeConstruct() override;

	/** Unbinds them. */
	virtual void NativeDestruct() override;

private:
	/** Field callback: re-validates. */
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	/** Field callback: Enter starts when valid. */
	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Shows or hides the error and enables Start for the field's current text. */
	void Validate();

	/** Broadcasts OnStart if the callsign is valid. */
	void TryStart();

	/** The trimmed field text. */
	FString GetCallsign() const;

	/** "Callsign". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadingText;

	/** "Slot 2". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SlotText;

	/** The callsign field. */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> CallsignField;

	/** "Use 3–16 letters, digits or spaces". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ErrorText;

	/** Creates the game. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> StartButton;

	/** Back to the slots. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> BackButton;
};
