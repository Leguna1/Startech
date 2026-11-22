#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "QuestListWidget.generated.h"

class UHubButtonWidget;
class UQuestListRowWidget;
class UVerticalBox;

/** Broadcast when the selected quest changes; None when the list is empty. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestSelected, FName);

/**
 * The hub's quest list, in a scroll box, grouped under headers in secondary text (11 pt):
 * - Active: accepted quests, ready or in progress.
 * - Available: story and timed quests that can be accepted, and repeatable ones waiting out their cooldown.
 * - Bounty board: the board's current offers ("Bounty board · refreshes when you dock").
 * - Completed (N): behind a Show toggle, collapsed at first.
 * One quest is selected at a time; the first row is selected on open, and again when the selected quest leaves.
 * The owner calls Rebuild and RefreshQuest from the quest events; nothing is polled except the cooldown rows.
 */
UCLASS()
class STARTECH_API UQuestListWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Rebuilds every group from the quest subsystem and the bounty board, keeping the selection if still listed. */
	void Rebuild();

	/** Re-reads one quest's row (progress or timer), if it is listed. */
	void RefreshQuest(FName QuestId);

	/** Re-reads every row showing a cooldown. */
	void RefreshCoolingRows();

	/** True when any row shows a cooldown. */
	bool HasCoolingRows() const;

	/** The selected quest, or None. */
	FName GetSelectedQuest() const { return SelectedQuest; }

	/** Fires when the selection changes. */
	FOnQuestSelected OnQuestSelected;

protected:
	/** A scroll box holding a vertical box of headers and rows. */
	virtual void BuildContent() override;

private:
	/** Adds a group header. */
	void AddHeader(const FText& Text);

	/** Adds a row for the quest. */
	void AddRow(FName QuestId);

	/** Adds a secondary-text line, for an empty group. */
	void AddNote(const FText& Text);

	/** Adds the "Completed (N)" header with its Show / Hide toggle. */
	void AddCompletedHeader(int32 Count);

	/** Selects QuestId (None for nothing), updates the rows' borders and broadcasts on a change. */
	void Select(FName QuestId);

	/** A row was clicked. */
	void HandleRowClicked(FName QuestId);

	/** The Show / Hide toggle was clicked. */
	void HandleToggleCompleted();

	/** The rows' container. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	/** Every row, in order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UQuestListRowWidget>> Rows;

	/** The selected quest. */
	FName SelectedQuest;

	/** The Completed group is expanded. */
	bool bShowCompleted = false;
};
