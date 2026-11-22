#include "Objectives/QuestObjective_Collect.h"

#include "QuestBridge.h"

#define LOCTEXT_NAMESPACE "QuestObjective"

bool UQuestObjective_Collect::TracksInventory() const
{
	return true;
}

int32 UQuestObjective_Collect::QueryCurrent(const IQuestInventoryProvider& Inventory) const
{
	return FMath::Clamp(Inventory.GetItemCount(ItemTag), 0, RequiredCount);
}

bool UQuestObjective_Collect::NeedsInventoryOnTurnIn() const
{
	return bConsumeOnTurnIn;
}

bool UQuestObjective_Collect::ApplyTurnIn(IQuestInventoryProvider& Inventory) const
{
	return !bConsumeOnTurnIn || Inventory.RemoveItems(ItemTag, RequiredCount);
}

bool UQuestObjective_Collect::IsObjectiveValid(FText& OutError) const
{
	if (!Super::IsObjectiveValid(OutError))
	{
		return false;
	}
	if (!ItemTag.IsValid())
	{
		OutError = LOCTEXT("NoItem", "A Collect objective needs an item tag.");
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
