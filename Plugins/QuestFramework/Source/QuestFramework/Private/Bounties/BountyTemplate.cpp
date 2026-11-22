#include "Bounties/BountyTemplate.h"

#include "QuestTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "BountyTemplate"

UBountyTemplate::UBountyTemplate()
{
	Category = QuestTags::Category_Bounty;
	TitleFormat = LOCTEXT("DefaultTitle", "Destroy {Count} {Target} in {Location}");
	ObjectiveFormat = LOCTEXT("DefaultObjective", "{Target} destroyed: {Current} / {Required}");
}

#if WITH_EDITOR
EDataValidationResult UBountyTemplate::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const auto Fail = [&Context, &Result](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};

	if (TemplateId.IsNone())
	{
		Fail(LOCTEXT("NoId", "TemplateId is not set."));
	}
	if (Targets.IsEmpty())
	{
		Fail(LOCTEXT("NoTargets", "The template has no targets."));
	}

	float TargetWeight = 0.f;
	bool bPaysPerUnit = false;
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		const FBountyTargetOption& Target = Targets[Index];
		TargetWeight += FMath::Max(Target.Weight, 0.f);
		bPaysPerUnit |= Target.RewardPerUnit > 0;
		if (Target.SubjectTags.IsEmpty())
		{
			Fail(FText::Format(LOCTEXT("NoSubject", "Target {0} has no subject tags."), FText::AsNumber(Index)));
		}
		if (Target.MinCount <= 0 || Target.MinCount > Target.MaxCount)
		{
			Fail(FText::Format(LOCTEXT("BadCount", "Target {0} needs 0 < MinCount <= MaxCount."), FText::AsNumber(Index)));
		}
	}
	if (!Targets.IsEmpty() && TargetWeight <= 0.f)
	{
		Fail(LOCTEXT("TargetWeight", "The targets' weights add up to 0."));
	}

	float LocationWeight = 0.f;
	for (const FBountyLocationOption& Location : Locations)
	{
		LocationWeight += FMath::Max(Location.Weight, 0.f);
	}
	if (!Locations.IsEmpty() && LocationWeight <= 0.f)
	{
		Fail(LOCTEXT("LocationWeight", "The locations' weights add up to 0."));
	}

	if (MaxTimeLimitSeconds > 0.f && MinTimeLimitSeconds > MaxTimeLimitSeconds)
	{
		Fail(LOCTEXT("TimeRange", "MinTimeLimitSeconds is above MaxTimeLimitSeconds."));
	}
	if (bPaysPerUnit && !CreditRewardTag.IsValid())
	{
		Fail(LOCTEXT("NoCreditTag", "Targets pay a reward but CreditRewardTag is not set."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
