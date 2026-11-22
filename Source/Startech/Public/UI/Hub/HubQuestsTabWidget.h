#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "QuestTypes.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubQuestsTabWidget.generated.h"

class UBountyBoardSubsystem;
class UQuestDetailsWidget;
class UQuestListWidget;
class UQuestSubsystem;

/**
 * The hub's Quests tab: the quest list on the left (about 45%) and the selected quest's details on the right, each on
 * a cut-corner panel. It listens to the quest events (OnQuestStatusChanged, OnObjectiveProgress, OnQuestTimeChanged,
 * OnQuestUnregistered) and the bounty board's OnBoardRefreshed, and updates the list and details from them; nothing
 * refreshes every frame. Cooldowns have no event, so while a cooldown is listed a one-second timer redraws those rows.
 */
UCLASS()
class STARTECH_API UHubQuestsTabWidget : public UHubWidgetBase
{
	GENERATED_BODY()

protected:
	/** The list and details side by side. */
	virtual void BuildContent() override;

	/** Binds to the quest events and fills the list (selecting the first row). */
	virtual void NativeConstruct() override;

	/** Unbinds and stops the cooldown timer. */
	virtual void NativeDestruct() override;

private:
	/** Rebuilds the list and refreshes the details. */
	UFUNCTION()
	void HandleQuestStatusChanged(FName QuestId, EQuestStatus NewStatus);

	/** Updates the quest's row, and the details when it is shown. */
	UFUNCTION()
	void HandleObjectiveProgress(FName QuestId, int32 ObjectiveIndex, int32 Current, int32 Required);

	/** Updates the quest's timer in its row and the details. */
	UFUNCTION()
	void HandleQuestTimeChanged(FName QuestId, float SecondsRemaining);

	/** Rebuilds the list and refreshes the details. */
	UFUNCTION()
	void HandleQuestUnregistered(FName QuestId);

	/** Rebuilds the list and refreshes the details. */
	UFUNCTION()
	void HandleBoardRefreshed();

	/** The list's selection changed. */
	void HandleQuestSelected(FName QuestId);

	/** Once a second while cooldowns are listed. */
	void HandleCooldownTick();

	/** Rebuilds the list, refreshes the details and starts or stops the cooldown timer. */
	void RebuildAll();

	/** Runs the cooldown timer exactly while the list shows a cooldown. */
	void UpdateCooldownTimer();

	/** The quest list. */
	UPROPERTY(Transient)
	TObjectPtr<UQuestListWidget> List;

	/** The selected quest's details. */
	UPROPERTY(Transient)
	TObjectPtr<UQuestDetailsWidget> Details;

	/** The quest subsystem bound to. */
	TWeakObjectPtr<UQuestSubsystem> BoundQuests;

	/** The bounty board bound to. */
	TWeakObjectPtr<UBountyBoardSubsystem> BoundBoard;

	/** Redraws cooldown rows once a second. */
	FTimerHandle CooldownTimer;
};
