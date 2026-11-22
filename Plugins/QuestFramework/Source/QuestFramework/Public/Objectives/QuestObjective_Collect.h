#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objectives/QuestObjective.h"
#include "QuestObjective_Collect.generated.h"

/**
 * Hold RequiredCount of an item. Progress is the current inventory count (capped), re-read on accept, on
 * Quest.Event.ItemCollected events and on UQuestSubsystem::RefreshInventoryObjectives, so selling or losing items
 * lowers it again.
 */
UCLASS(meta = (DisplayName = "Collect"))
class QUESTFRAMEWORK_API UQuestObjective_Collect : public UQuestObjective
{
	GENERATED_BODY()

public:
	/** The item to hold, as the game's inventory provider names it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FGameplayTag ItemTag;

	/** If true, turning the quest in takes RequiredCount of the item from the inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	bool bConsumeOnTurnIn = true;

	/** Always true: progress comes from the inventory. */
	virtual bool TracksInventory() const override;

	/** The inventory's count of ItemTag, capped at RequiredCount. */
	virtual int32 QueryCurrent(const IQuestInventoryProvider& Inventory) const override;

	/** bConsumeOnTurnIn. */
	virtual bool NeedsInventoryOnTurnIn() const override;

	/** Removes RequiredCount of the item when bConsumeOnTurnIn is set. */
	virtual bool ApplyTurnIn(IQuestInventoryProvider& Inventory) const override;

	/** Also needs a valid item tag. */
	virtual bool IsObjectiveValid(FText& OutError) const override;
};
