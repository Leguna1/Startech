#include "Objectives/QuestObjective_Visit.h"

#include "QuestTags.h"
#include "QuestTypes.h"

#define LOCTEXT_NAMESPACE "QuestObjective"

int32 UQuestObjective_Visit::EvaluateEvent(const FQuestEvent& Event) const
{
	const bool bMatches = Event.EventTag.MatchesTagExact(QuestTags::Event_LocationReached) && !ContextId.IsNone()
		&& Event.ContextId == ContextId;
	return bMatches ? FMath::Max(Event.Amount, 0) : 0;
}

bool UQuestObjective_Visit::IsObjectiveValid(FText& OutError) const
{
	if (!Super::IsObjectiveValid(OutError))
	{
		return false;
	}
	if (ContextId.IsNone())
	{
		OutError = LOCTEXT("NoContext", "A Visit objective needs a context id.");
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
