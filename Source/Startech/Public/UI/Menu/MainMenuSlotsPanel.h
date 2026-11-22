#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuSlotsPanel.generated.h"

class UHubButtonWidget;
class UMainMenuSlotCard;
class UTextBlock;
class UVerticalBox;

/** Broadcast with a slot index. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMenuSlotAction, int32 /* SlotIndex */);

/** What the slot page is for. */
enum class EMainMenuSlotMode : uint8
{
	/** Every visible slot, each with Select. */
	NewGame,
	/** Occupied slots only, each with Load and Delete. */
	Load
};

/**
 * The main menu's slot page: a heading, one UMainMenuSlotCard per slot, a status line for errors, and Back. Used for
 * New game (every slot, Select) and Load (occupied slots, Load and Delete).
 */
UCLASS()
class STARTECH_API UMainMenuSlotsPanel : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Rebuilds the cards for Mode from Infos and clears the status line. */
	void Show(EMainMenuSlotMode Mode, const TArray<FSaveSlotInfo>& Infos);

	/** Shows Message in the status line (empty hides it). */
	void SetStatus(const FText& Message);

	/** Select (new game) or Load was clicked on a card. */
	FOnMenuSlotAction OnSlotPicked;

	/** Delete was clicked on a card. */
	FOnMenuSlotAction OnDeleteRequested;

	/** Back was clicked. */
	FSimpleMulticastDelegate OnBack;

protected:
	/** The heading, the card column, the status line and Back, in a fixed-width column. */
	virtual void BuildContent() override;

	/** Binds Back. */
	virtual void NativeConstruct() override;

	/** Unbinds Back and the cards. */
	virtual void NativeDestruct() override;

private:
	/** Removes every card. */
	void ClearCards();

	/** "New game" or "Load". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadingText;

	/** Holds the cards. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> CardsBox;

	/** Errors such as a failed load. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	/** Back to the first page. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> BackButton;

	/** The cards shown. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMainMenuSlotCard>> Cards;
};
