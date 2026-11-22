#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MainMenuSlotCard.generated.h"

class UHubButtonWidget;
class UTextBlock;

/** Broadcast with the card's slot index. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSlotCardAction, int32 /* SlotIndex */);

/**
 * One save slot on the main menu: "Slot N", then "Empty", or the pilot name with "Played h:mm · Saved <date>", and
 * "Restored from backup" when the main file was damaged. For a new game it has a Select button; for loading, Load and
 * a quiet Delete. Made at runtime by UMainMenuSlotsPanel.
 */
UCLASS()
class STARTECH_API UMainMenuSlotCard : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Shows Info; bLoadMode picks Load + Delete instead of Select. */
	void Setup(const FSaveSlotInfo& InInfo, bool bInLoadMode);

	/** Select or Load was clicked. */
	FOnSlotCardAction OnPrimary;

	/** Delete was clicked. */
	FOnSlotCardAction OnDelete;

	/** "1:24": whole hours and minutes. */
	static FText FormatPlayTime(double Seconds);

protected:
	/** A panel with the texts on the left and the buttons on the right. */
	virtual void BuildContent() override;

	/** Forwards the buttons' clicks with the slot index. */
	virtual void NativeConstruct() override;

	/** Unbinds the buttons. */
	virtual void NativeDestruct() override;

private:
	/** Writes Info into the texts and buttons. */
	void Apply();

	/** The slot shown. */
	FSaveSlotInfo Info;

	/** Load + Delete rather than Select. */
	bool bLoadMode = false;

	/** "Slot 1". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SlotText;

	/** The pilot, or "Empty". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	/** "Played 1:24 · Saved 21 Nov 2026 14:05". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailText;

	/** "Restored from backup". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BackupText;

	/** Select or Load. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> PrimaryButton;

	/** Delete (load mode only). */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> DeleteButton;
};
