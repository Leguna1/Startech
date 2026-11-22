#include "Bounties/BountyFactory.h"

#include "Bounties/BountyTemplate.h"
#include "Definitions/QuestDefinition.h"
#include "Math/RandomStream.h"
#include "Objectives/QuestObjective_Destroy.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "BountyFactory"

UQuestDefinition* FBountyFactory::Generate(const UBountyTemplate& Template, const int32 Seed, UObject* const Outer)
{
	if (Template.Targets.IsEmpty())
	{
		return nullptr;
	}

	// The draw order is fixed: target, count, location, time. Add new draws at the end only, or saved bounties change.
	const FRandomStream Stream(Seed);

	const FBountyTargetOption& Target = Template.Targets[PickWeighted(Template.Targets, Stream)];
	const int32 MinCount = FMath::Max(1, FMath::Min(Target.MinCount, Target.MaxCount));
	const int32 Count = Stream.RandRange(MinCount, FMath::Max(MinCount, Target.MaxCount));

	const int32 LocationIndex = PickWeighted(Template.Locations, Stream);
	const FBountyLocationOption* const Location = Template.Locations.IsValidIndex(LocationIndex) ? &Template.Locations[LocationIndex] : nullptr;

	const float TimeDraw = Stream.FRand();
	const float TimeLimit = Template.MaxTimeLimitSeconds > 0.f
		? FMath::RoundToFloat(FMath::Lerp(FMath::Min(Template.MinTimeLimitSeconds, Template.MaxTimeLimitSeconds), Template.MaxTimeLimitSeconds, TimeDraw))
		: 0.f;

	FFormatNamedArguments Args;
	Args.Add(TEXT("Count"), FText::AsNumber(Count));
	Args.Add(TEXT("Target"), Target.DisplayName);
	Args.Add(TEXT("Location"), Location ? Location->DisplayName : LOCTEXT("Anywhere", "anywhere"));

	const FName QuestId = MakeQuestId(Template.TemplateId, Seed);
	UObject* const DefinitionOuter = Outer ? Outer : GetTransientPackage();
	UQuestDefinition* const Definition = NewObject<UQuestDefinition>(DefinitionOuter,
		MakeUniqueObjectName(DefinitionOuter, UQuestDefinition::StaticClass(), QuestId), RF_Transient);

	Definition->QuestId = QuestId;
	Definition->Title = FText::Format(Template.TitleFormat, Args);
	Definition->Description = Template.DescriptionFormat.IsEmpty() ? FText::GetEmpty() : FText::Format(Template.DescriptionFormat, Args);
	Definition->Category = Template.Category;
	Definition->TimeLimitSeconds = TimeLimit;
	Definition->bRequiresTurnIn = Template.bRequiresTurnIn;
	Definition->SourceTemplateId = Template.TemplateId;
	Definition->GenerationSeed = Seed;

	UQuestObjective_Destroy* const Objective = NewObject<UQuestObjective_Destroy>(Definition);
	Objective->TargetTags = Target.SubjectTags;
	Objective->RequiredContextId = Location ? Location->ContextId : NAME_None;
	Objective->RequiredCount = Count;
	// {Current} and {Required} have no argument here, so they stay in the text for GetProgressText.
	Objective->Description = FText::Format(Template.ObjectiveFormat, Args);
	Definition->Objectives.Add(Objective);

	const float Multiplier = Location ? FMath::Max(Location->RewardMultiplier, 0.f) : 1.f;
	const int32 Credits = FMath::RoundToInt(static_cast<float>(Count * Target.RewardPerUnit) * Multiplier);
	if (Credits > 0 && Template.CreditRewardTag.IsValid())
	{
		FQuestReward& Reward = Definition->Rewards.AddDefaulted_GetRef();
		Reward.RewardTag = Template.CreditRewardTag;
		Reward.Amount = Credits;
	}
	Definition->Rewards.Append(Template.ExtraRewards);

	return Definition;
}

FName FBountyFactory::MakeQuestId(const FName TemplateId, const int32 Seed)
{
	return FName(*FString::Printf(TEXT("Bounty_%s_%d"), *TemplateId.ToString(), Seed));
}

template <typename OptionType>
int32 FBountyFactory::PickWeighted(const TArray<OptionType>& Options, const FRandomStream& Stream)
{
	// Always one draw, even with nothing to pick, so the sequence after it never shifts.
	const float Draw = Stream.FRand();
	if (Options.IsEmpty())
	{
		return INDEX_NONE;
	}

	float Total = 0.f;
	for (const OptionType& Option : Options)
	{
		Total += FMath::Max(Option.Weight, 0.f);
	}
	if (Total <= 0.f)
	{
		return FMath::Min(FMath::FloorToInt(Draw * Options.Num()), Options.Num() - 1);
	}

	float Remaining = Draw * Total;
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		Remaining -= FMath::Max(Options[Index].Weight, 0.f);
		if (Remaining < 0.f)
		{
			return Index;
		}
	}
	return Options.Num() - 1;
}

#undef LOCTEXT_NAMESPACE
