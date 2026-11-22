#include "Objectives/QuestObjective_Destroy.h"

#include "QuestTags.h"
#include "QuestTypes.h"

#define LOCTEXT_NAMESPACE "QuestObjective"

int32 UQuestObjective_Destroy::EvaluateEvent(const FQuestEvent& Event) const
{
	if (!Event.EventTag.MatchesTagExact(QuestTags::Event_TargetDestroyed) || !Event.SubjectTags.HasAny(TargetTags))
	{
		return 0;
	}
	if (!RequiredContextId.IsNone() && Event.ContextId != RequiredContextId)
	{
		return 0;
	}
	return FMath::Max(Event.Amount, 0);
}

bool UQuestObjective_Destroy::IsObjectiveValid(FText& OutError) const
{
	if (!Super::IsObjectiveValid(OutError))
	{
		return false;
	}
	if (TargetTags.IsEmpty())
	{
		OutError = LOCTEXT("NoTargets", "A Destroy objective needs at least one target tag.");
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
