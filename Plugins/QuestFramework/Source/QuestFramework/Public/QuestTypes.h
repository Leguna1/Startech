#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.generated.h"

/** Where a quest stands for the player. */
UENUM(BlueprintType)
enum class EQuestStatus : uint8
{
	/** At least one prerequisite quest has not been completed yet. */
	Locked,
	/** Can be accepted. */
	Available,
	/** Accepted; its objectives count events. */
	Active,
	/** Every objective is done; waiting for the player to turn it in. */
	ReadyToTurnIn,
	/** Turned in (or completed automatically). A repeatable quest stays here during its cooldown. */
	Completed,
	/** Its time ran out. A repeatable quest stays here during its cooldown; others return to Available at once. */
	Failed
};

/**
 * Something that happened in the game, reported to UQuestSubsystem::ReportEvent. The plugin never sees game classes:
 * objectives decide from the tags alone whether an event counts.
 */
USTRUCT(BlueprintType)
struct QUESTFRAMEWORK_API FQuestEvent
{
	GENERATED_BODY()

	/** What happened: Quest.Event.TargetDestroyed, Quest.Event.ItemCollected, Quest.Event.LocationReached, or a game's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (Categories = "Quest.Event"))
	FGameplayTag EventTag;

	/** What it happened to: what was destroyed or collected, for example Enemy.Bandit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FGameplayTagContainer SubjectTags;

	/** Where it happened, for example a zone or region id. None when unknown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName ContextId;

	/** How many times it happened. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 Amount = 1;
};

/** One reward. The game decides what the tag means when it grants it (credits, an item, reputation). */
USTRUCT(BlueprintType)
struct QUESTFRAMEWORK_API FQuestReward
{
	GENERATED_BODY()

	/** Game-defined kind of reward, for example Reward.Credits or Reward.Item.IronOre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FGameplayTag RewardTag;

	/** How much of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "0"))
	int32 Amount = 0;
};

/**
 * Everything that changes about one quest while playing. Plain values only, so an array of these is the whole quest
 * save: see UQuestSubsystem::GetAllStates and RestoreStates.
 */
USTRUCT(BlueprintType)
struct QUESTFRAMEWORK_API FQuestRuntimeState
{
	GENERATED_BODY()

	/** The quest this state belongs to. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	/** Where the quest stands. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestStatus Status = EQuestStatus::Locked;

	/** Current count of each objective, in the definition's order, capped at its RequiredCount. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<int32> ObjectiveProgress;

	/** Seconds left on an active timed quest. Stored as time left rather than a timestamp, so it saves cleanly. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	float TimeRemainingSeconds = 0.f;

	/** How many times the quest has been completed. Prerequisites are met once this is above zero. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 TimesCompleted = 0;

	/** Seconds before a repeatable quest can be accepted again. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	float CooldownRemainingSeconds = 0.f;

	/** For a generated bounty: the template it came from, so it can be regenerated after loading. None otherwise. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	FName SourceTemplateId;

	/** For a generated bounty: the seed it was generated with. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Quest")
	int32 GenerationSeed = 0;
};

/** The most quests of one category that may be active (or ready to turn in) at once. */
USTRUCT(BlueprintType)
struct QUESTFRAMEWORK_API FQuestCategoryLimit
{
	GENERATED_BODY()

	/** The category, for example Quest.Category.Bounty. Matched exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (Categories = "Quest.Category"))
	FGameplayTag Category;

	/** The limit. A category with no entry has none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "0"))
	int32 Max = 1;

	/**
	 * Plural name of the category's quests in messages, for example "bounties" in "Limit reached: 3 bounties".
	 * When empty, the last segment of the category tag is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText DisplayName;
};

/** One kind of target a bounty template can ask for. */
USTRUCT(BlueprintType)
struct QUESTFRAMEWORK_API FBountyTargetOption
{
	GENERATED_BODY()

	/** What has to be destroyed; an event matches if its subject has any of these (parents match children). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty")
	FGameplayTagContainer SubjectTags;

	/** Name used for {Target} in the title, for example "bandits". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty")
	FText DisplayName;

	/** Relative chance of being picked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Fewest targets asked for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty", meta = (ClampMin = "1"))
	int32 MinCount = 3;

	/** Most targets asked for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty", meta = (ClampMin = "1"))
	int32 MaxCount = 6;

	/** Reward per target, before the location's multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty", meta = (ClampMin = "0"))
	int32 RewardPerUnit = 100;
};

/** One place a bounty template can send the player. */
USTRUCT(BlueprintType)
struct QUESTFRAMEWORK_API FBountyLocationOption
{
	GENERATED_BODY()

	/** The context id the kills must happen in. None means anywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty")
	FName ContextId;

	/** Name used for {Location} in the title, for example "the northern marsh" or "anywhere". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty")
	FText DisplayName;

	/** Relative chance of being picked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Multiplies the target reward, so dangerous places pay more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounty", meta = (ClampMin = "0"))
	float RewardMultiplier = 1.f;
};
