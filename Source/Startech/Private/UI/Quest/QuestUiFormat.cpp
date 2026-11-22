#include "UI/Quest/QuestUiFormat.h"

#include "Definitions/QuestDefinition.h"
#include "HudFormat.h"
#include "Objectives/QuestObjective.h"
#include "Objectives/QuestObjective_Collect.h"
#include "QuestTags.h"
#include "QuestTypes.h"
#include "StartechGameplayTags.h"
#include "StartechSettings.h"
#include "TradeableData.h"
#include "UI/HUD/StartechHudStyle.h"

#define LOCTEXT_NAMESPACE "QuestUiFormat"

namespace QuestUiFormat
{
	/** The last segment of a tag: "Ferrite" for Item.Mineral.Ferrite. */
	FText LastSegment(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		int32 Dot = INDEX_NONE;
		if (Name.FindLastChar(TEXT('.'), Dot))
		{
			Name.RightChopInline(Dot + 1);
		}
		return FText::FromString(Name);
	}
}

FText FQuestUiFormat::CategoryName(const UQuestDefinition& Definition)
{
	return QuestUiFormat::LastSegment(Definition.Category);
}

FLinearColor FQuestUiFormat::CategoryColor(const UQuestDefinition& Definition, const UStartechHudStyle& Style)
{
	if (Definition.Category == QuestTags::Category_Story)
	{
		return Style.PlayerColor;
	}
	if (Definition.Category == QuestTags::Category_Timed)
	{
		return Style.HullColor;
	}
	if (Definition.Category == QuestTags::Category_Bounty)
	{
		return Style.CreditsColor;
	}
	return Style.TextColor;
}

int32 FQuestUiFormat::CategoryRank(const UQuestDefinition& Definition)
{
	if (Definition.Category == QuestTags::Category_Timed)
	{
		return 0;
	}
	if (Definition.Category == QuestTags::Category_Story)
	{
		return 1;
	}
	if (Definition.Category == QuestTags::Category_Bounty)
	{
		return 2;
	}
	return 3;
}

FLinearColor FQuestUiFormat::TimerColor(const float Seconds, const UStartechHudStyle& Style)
{
	if (Seconds < Style.QuestTimerCriticalSeconds)
	{
		return Style.DangerColor;
	}
	if (Seconds < Style.QuestTimerWarnSeconds)
	{
		return Style.HullColor;
	}
	return Style.TextColor;
}

bool FQuestUiFormat::IsTimerRunning(const UQuestDefinition& Definition, const FQuestRuntimeState& State)
{
	return State.Status == EQuestStatus::Active && Definition.HasTimeLimit();
}

bool FQuestUiFormat::IsCoolingDown(const FQuestRuntimeState& State)
{
	return State.CooldownRemainingSeconds > 0.f
		&& (State.Status == EQuestStatus::Completed || State.Status == EQuestStatus::Failed);
}

int32 FQuestUiFormat::GetProgress(const FQuestRuntimeState& State, const int32 ObjectiveIndex)
{
	return State.ObjectiveProgress.IsValidIndex(ObjectiveIndex) ? State.ObjectiveProgress[ObjectiveIndex] : 0;
}

int32 FQuestUiFormat::GetRequired(const UQuestDefinition& Definition, const int32 ObjectiveIndex)
{
	const UQuestObjective* const Objective = Definition.Objectives.IsValidIndex(ObjectiveIndex)
		? Definition.Objectives[ObjectiveIndex].Get() : nullptr;
	return Objective ? Objective->RequiredCount : 0;
}

bool FQuestUiFormat::IsObjectiveDone(const UQuestDefinition& Definition, const FQuestRuntimeState& State,
	const int32 ObjectiveIndex)
{
	const int32 Required = GetRequired(Definition, ObjectiveIndex);
	return Required > 0 && GetProgress(State, ObjectiveIndex) >= Required;
}

int32 FQuestUiFormat::CountDoneObjectives(const UQuestDefinition& Definition, const FQuestRuntimeState& State)
{
	int32 Done = 0;
	for (int32 Index = 0; Index < Definition.Objectives.Num(); ++Index)
	{
		Done += IsObjectiveDone(Definition, State, Index) ? 1 : 0;
	}
	return Done;
}

int32 FQuestUiFormat::FindCurrentObjective(const UQuestDefinition& Definition, const FQuestRuntimeState& State)
{
	for (int32 Index = 0; Index < Definition.Objectives.Num(); ++Index)
	{
		if (Definition.Objectives[Index] && !IsObjectiveDone(Definition, State, Index))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FText FQuestUiFormat::ObjectiveText(const UQuestDefinition& Definition, const FQuestRuntimeState& State,
	const int32 ObjectiveIndex)
{
	const UQuestObjective* const Objective = Definition.Objectives.IsValidIndex(ObjectiveIndex)
		? Definition.Objectives[ObjectiveIndex].Get() : nullptr;
	return Objective ? Objective->GetProgressText(GetProgress(State, ObjectiveIndex)) : FText::GetEmpty();
}

FText FQuestUiFormat::Summary(const UQuestDefinition& Definition, const FQuestRuntimeState& State)
{
	const FText Category = CategoryName(Definition);
	const bool bAccepted = State.Status == EQuestStatus::Active || State.Status == EQuestStatus::ReadyToTurnIn;
	const int32 Count = Definition.Objectives.Num();

	if (Count == 1)
	{
		// Before accepting, the line reads with a zero count, which still tells what the quest asks for.
		return FText::Format(LOCTEXT("SummaryOne", "{0} · {1}"), Category, ObjectiveText(Definition, State, 0));
	}
	if (!bAccepted)
	{
		return FText::Format(LOCTEXT("SummaryCount", "{0} · {1} {1}|plural(one=objective,other=objectives)"),
			Category, FText::AsNumber(Count));
	}
	return FText::Format(LOCTEXT("SummaryProgress", "{0} · {1}/{2} objectives"), Category,
		FText::AsNumber(CountDoneObjectives(Definition, State)), FText::AsNumber(Count));
}

FText FQuestUiFormat::TimeLimitText(const UQuestDefinition& Definition, const FQuestRuntimeState& State)
{
	if (!Definition.HasTimeLimit())
	{
		return FText::GetEmpty();
	}
	if (IsTimerRunning(Definition, State))
	{
		return FText::Format(LOCTEXT("TimeLeft", "Time left {0}"), FHudFormat::Countdown(State.TimeRemainingSeconds));
	}
	return FText::Format(LOCTEXT("TimeLimit", "Time limit {0}"), FHudFormat::Countdown(Definition.TimeLimitSeconds));
}

bool FQuestUiFormat::IsCreditsReward(const FQuestReward& Reward)
{
	return Reward.RewardTag == StartechTags::Reward_Credits;
}

FText FQuestUiFormat::RewardText(const FQuestReward& Reward)
{
	if (IsCreditsReward(Reward))
	{
		return FText::Format(LOCTEXT("RewardCredits", "{0} credits"), FText::AsNumber(Reward.Amount));
	}
	return FText::Format(LOCTEXT("RewardItem", "{0} × {1}"), FText::AsNumber(Reward.Amount), ItemName(Reward.RewardTag));
}

int32 FQuestUiFormat::CreditsReward(const UQuestDefinition& Definition)
{
	int32 Total = 0;
	for (const FQuestReward& Reward : Definition.Rewards)
	{
		Total += IsCreditsReward(Reward) ? Reward.Amount : 0;
	}
	return Total;
}

FText FQuestUiFormat::ItemName(const FGameplayTag& ItemTag)
{
	const UTradeableData* const Good = UStartechSettings::Get().FindItem(ItemTag);
	return Good && !Good->DisplayName.IsEmpty() ? Good->DisplayName : QuestUiFormat::LastSegment(ItemTag);
}

FText FQuestUiFormat::HandoverText(const UQuestDefinition& Definition)
{
	TArray<FText> Lines;
	for (const UQuestObjective* const Objective : Definition.Objectives)
	{
		const UQuestObjective_Collect* const Collect = Cast<UQuestObjective_Collect>(Objective);
		if (Collect && Collect->NeedsInventoryOnTurnIn())
		{
			Lines.Add(FText::Format(LOCTEXT("Handover", "{0} {1} will be handed over"),
				FText::AsNumber(Collect->RequiredCount), ItemName(Collect->ItemTag)));
		}
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#undef LOCTEXT_NAMESPACE
