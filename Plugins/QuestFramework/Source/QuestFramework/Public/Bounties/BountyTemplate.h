#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.h"
#include "BountyTemplate.generated.h"

/**
 * The recipe for generated bounties: which targets, how many, where, for how long and for what reward.
 * FBountyFactory turns a template and a seed into a quest definition with one Destroy objective.
 */
UCLASS(BlueprintType)
class QUESTFRAMEWORK_API UBountyTemplate : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Sets the category to Quest.Category.Bounty. */
	UBountyTemplate();

	/** Unique id; part of every generated quest id (Bounty_<TemplateId>_<Seed>) and stored in saves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty")
	FName TemplateId;

	/** Category of the generated quests. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty", meta = (Categories = "Quest.Category"))
	FGameplayTag Category;

	/** Title of a generated quest. {Count}, {Target} and {Location} are filled in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText TitleFormat;

	/** Description of a generated quest, with the same arguments as the title. May be empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text", meta = (MultiLine = "true"))
	FText DescriptionFormat;

	/** The objective's text. {Target} and {Location} are filled in when generated; {Current} and {Required} as it progresses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FText ObjectiveFormat;

	/** The kinds of target a bounty may ask for; one is picked by weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty")
	TArray<FBountyTargetOption> Targets;

	/** The places a bounty may be set in; one is picked by weight. Empty means anywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty")
	TArray<FBountyLocationOption> Locations;

	/** Shortest time limit, in whole seconds. With MaxTimeLimitSeconds at 0 the bounties are untimed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty", meta = (ClampMin = "0", Units = "s"))
	float MinTimeLimitSeconds = 0.f;

	/** Longest time limit; 0 means no time limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty", meta = (ClampMin = "0", Units = "s"))
	float MaxTimeLimitSeconds = 0.f;

	/** The reward tag the per-target reward is given as, for example Reward.Credits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	FGameplayTag CreditRewardTag;

	/** Fixed rewards added to every generated bounty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	TArray<FQuestReward> ExtraRewards;

	/** Whether the generated bounties have to be turned in (true) or complete when the last target is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounty")
	bool bRequiresTurnIn = true;

#if WITH_EDITOR
	/**
	 * Checks the template id, at least one target with subject tags and MinCount no greater than MaxCount, a positive
	 * total weight for targets (and locations, if any), the time range, and a credit tag when targets pay a reward.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
