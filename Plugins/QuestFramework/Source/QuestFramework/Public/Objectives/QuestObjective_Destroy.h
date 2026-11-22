#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Objectives/QuestObjective.h"
#include "QuestObjective_Destroy.generated.h"

/** Destroy RequiredCount things: counts Quest.Event.TargetDestroyed events whose subject matches TargetTags. */
UCLASS(meta = (DisplayName = "Destroy"))
class QUESTFRAMEWORK_API UQuestObjective_Destroy : public UQuestObjective
{
	GENERATED_BODY()

public:
	/** An event counts if its subject has any of these. A parent tag matches its children (Enemy matches Enemy.Bandit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FGameplayTagContainer TargetTags;

	/** If set, only events with this context id count (for example, kills in one region). None counts them anywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName RequiredContextId;

	/** The event's Amount for a matching TargetDestroyed event, else 0. */
	virtual int32 EvaluateEvent(const FQuestEvent& Event) const override;

	/** Also needs at least one target tag. */
	virtual bool IsObjectiveValid(FText& OutError) const override;
};
