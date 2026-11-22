#include "Definitions/QuestDefinition.h"

#include "Objectives/QuestObjective.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "QuestDefinition"

const FPrimaryAssetType UQuestDefinition::PrimaryAssetType(TEXT("QuestDefinition"));

FPrimaryAssetId UQuestDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UQuestDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (QuestId.IsNone())
	{
		Fail(LOCTEXT("NoId", "QuestId is not set."));
	}
	if (Objectives.IsEmpty())
	{
		Fail(LOCTEXT("NoObjectives", "The quest has no objectives."));
	}

	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const UQuestObjective* const Objective = Objectives[Index];
		if (!Objective)
		{
			Fail(FText::Format(LOCTEXT("EmptyObjective", "Objective {0} has no kind selected."), FText::AsNumber(Index)));
			continue;
		}

		// Each entry must be its own instance; a shared one would make two objectives one.
		const int32 FirstUse = Objectives.IndexOfByKey(Objective);
		if (FirstUse != Index)
		{
			Fail(FText::Format(LOCTEXT("SharedObjective", "Objectives {0} and {1} are the same object."),
				FText::AsNumber(FirstUse), FText::AsNumber(Index)));
		}

		FText Error;
		if (!Objective->IsObjectiveValid(Error))
		{
			Fail(FText::Format(LOCTEXT("BadObjective", "Objective {0}: {1}"), FText::AsNumber(Index), Error));
		}
	}

	if (!QuestId.IsNone() && PrerequisiteQuestIds.Contains(QuestId))
	{
		Fail(LOCTEXT("SelfPrerequisite", "The quest lists itself as a prerequisite."));
	}
	if (TimeLimitSeconds < 0.f || RepeatCooldownSeconds < 0.f)
	{
		Fail(LOCTEXT("NegativeTime", "Time limit and cooldown cannot be negative."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
