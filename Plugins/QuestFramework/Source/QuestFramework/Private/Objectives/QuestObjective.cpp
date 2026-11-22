#include "Objectives/QuestObjective.h"

#include "QuestBridge.h"
#include "QuestTypes.h"

#define LOCTEXT_NAMESPACE "QuestObjective"

int32 UQuestObjective::EvaluateEvent(const FQuestEvent& Event) const
{
	return 0;
}

bool UQuestObjective::TracksInventory() const
{
	return false;
}

int32 UQuestObjective::QueryCurrent(const IQuestInventoryProvider& Inventory) const
{
	return 0;
}

bool UQuestObjective::NeedsInventoryOnTurnIn() const
{
	return false;
}

bool UQuestObjective::ApplyTurnIn(IQuestInventoryProvider& Inventory) const
{
	return true;
}

FText UQuestObjective::GetProgressText(const int32 Current) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Current"), FText::AsNumber(Current));
	Args.Add(TEXT("Required"), FText::AsNumber(RequiredCount));

	const FText Format = Description.IsEmpty() ? LOCTEXT("DefaultProgress", "{Current} / {Required}") : Description;
	return FText::Format(Format, Args);
}

bool UQuestObjective::IsObjectiveValid(FText& OutError) const
{
	if (RequiredCount <= 0)
	{
		OutError = LOCTEXT("RequiredCount", "RequiredCount must be above 0.");
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
