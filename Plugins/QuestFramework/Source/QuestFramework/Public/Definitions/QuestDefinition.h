#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.h"
#include "QuestDefinition.generated.h"

class UQuestObjective;

/**
 * One quest, authored as a data asset (or generated at runtime by FBountyFactory). Read-only while playing: what
 * changes is kept in FQuestRuntimeState by UQuestSubsystem.
 */
UCLASS(BlueprintType)
class QUESTFRAMEWORK_API UQuestDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Primary asset type of every quest definition, "QuestDefinition". */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Unique id of the quest, used by the API, prerequisites and saves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	/** Name shown to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	/** Longer text shown to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = "true"))
	FText Description;

	/** The quest's kind, for example Quest.Category.Story. Category limits apply per category. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (Categories = "Quest.Category"))
	FGameplayTag Category;

	/** What the quest asks for. Pick a kind for each entry (Destroy, Collect, Visit). */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Objectives")
	TArray<TObjectPtr<UQuestObjective>> Objectives;

	/** When true, only the first incomplete objective counts events, so the objectives are done one after another. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objectives")
	bool bObjectivesInOrder = false;

	/** Seconds to finish once accepted; 0 means no limit. The quest fails when it runs out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0", Units = "s"))
	float TimeLimitSeconds = 0.f;

	/** Quests that must have been completed before this one can be accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<FName> PrerequisiteQuestIds;

	/** Can be accepted again after completing (or failing), once RepeatCooldownSeconds have passed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	bool bRepeatable = false;

	/** Seconds after completing or failing a repeatable quest before it can be accepted again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0", Units = "s", EditCondition = "bRepeatable"))
	float RepeatCooldownSeconds = 0.f;

	/** When true the player turns the quest in (UQuestSubsystem::TurnIn); when false it completes once every objective is done. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	bool bRequiresTurnIn = true;

	/** Given through the game's reward granter on completion. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	TArray<FQuestReward> Rewards;

	/** For a generated bounty: the template it came from. None for authored quests. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generated")
	FName SourceTemplateId;

	/** For a generated bounty: the seed it was generated with. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Generated")
	int32 GenerationSeed = 0;

	/** True if the quest was generated from a bounty template. */
	bool IsGenerated() const { return !SourceTemplateId.IsNone(); }

	/** True if the quest has a time limit. */
	bool HasTimeLimit() const { return TimeLimitSeconds > 0.f; }

	/** Type "QuestDefinition", named after the asset, so the Asset Manager can find quests too. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	/**
	 * Checks the quest id is set, there is at least one objective, every objective is set, used once and valid for
	 * its kind (RequiredCount above 0 among others), and the quest is not its own prerequisite.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
