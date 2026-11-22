#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "QuestDetailsWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UHubButtonWidget;
class UTextBlock;
class UVerticalBox;

/**
 * The selected quest in the hub, built in C++:
 * - the title (16 pt) and a category badge: Story in Player tones, Timed in Hull tones, Bounty in Credits tones;
 * - the description (12 pt, wrapped);
 * - each objective (UQuestObjectiveRowWidget);
 * - the time limit for timed quests ("Time limit 4:00", or "Time left 2:41" while active);
 * - the rewards (credits in the Credits colour, items as "200 × LL1") and the handover line for consumed items;
 * - the main button: Accept, Turn in, or "In progress" disabled. When the quest subsystem refuses (CanAccept or
 *   CanTurnIn), the button is disabled with the reason underneath, before the player clicks;
 * - Abandon, a quiet button for active quests, behind a confirmation ("Abandon First contract? Progress is lost.").
 * The owner calls Refresh and RefreshTimer from the quest events.
 */
UCLASS()
class STARTECH_API UQuestDetailsWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Shows QuestId (None shows nothing), closing any open confirmation. */
	void ShowQuest(FName InQuestId);

	/** The quest shown, or None. */
	FName GetQuestId() const { return QuestId; }

	/** Re-reads everything about the quest shown from the quest subsystem. */
	void Refresh();

	/** Re-reads the time line only, for the once-per-second clock event. */
	void RefreshTimer();

protected:
	/** The layout: a scroll box with every section, filled by Refresh. */
	virtual void BuildContent() override;

	/** Binds the buttons. */
	virtual void NativeConstruct() override;

	/** Unbinds the buttons. */
	virtual void NativeDestruct() override;

private:
	/** What the main button does. */
	enum class EMainAction : uint8
	{
		/** Nothing: hidden, or "In progress". */
		None,
		/** Accepts the quest. */
		Accept,
		/** Turns the quest in. */
		TurnIn
	};

	/** Accept or Turn in. */
	void HandleMainClicked();

	/** Opens the confirmation. */
	void HandleAbandonClicked();

	/** Abandons the quest. */
	void HandleConfirmAbandonClicked();

	/** Closes the confirmation. */
	void HandleKeepClicked();

	/** Adds a section header ("Objectives", "Rewards") to Body. */
	UTextBlock* AddSectionHeader(const FText& Text);

	/** The quest shown. */
	FName QuestId;

	/** What the main button does now. */
	EMainAction MainAction = EMainAction::None;

	/** The Abandon confirmation is open. */
	bool bConfirming = false;

	/** "Select a quest", shown when nothing is selected. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyText;

	/** Every section; hidden when nothing is selected. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Body;

	/** The title. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	/** The category badge's background. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Badge;

	/** The category badge's text. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BadgeText;

	/** The description. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;

	/** One UQuestObjectiveRowWidget per objective. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ObjectivesBox;

	/** "Time limit 4:00" / "Time left 2:41". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeText;

	/** One line per reward. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RewardsBox;

	/** "20 Ferrite will be handed over". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HandoverText;

	/** The main button and Abandon. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ActionRow;

	/** Accept, Turn in or "In progress". */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> MainButton;

	/** Opens the Abandon confirmation. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> AbandonButton;

	/** Why the main button is disabled. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReasonText;

	/** The Abandon confirmation. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ConfirmBox;

	/** "Abandon First contract? Progress is lost." */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;

	/** Confirms abandoning. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ConfirmAbandonButton;

	/** Closes the confirmation. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> KeepButton;
};
