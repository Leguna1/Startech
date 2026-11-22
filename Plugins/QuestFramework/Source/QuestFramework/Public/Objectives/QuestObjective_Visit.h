#pragma once

#include "CoreMinimal.h"
#include "Objectives/QuestObjective.h"
#include "QuestObjective_Visit.generated.h"

/** Go somewhere: counts Quest.Event.LocationReached events with ContextId. RequiredCount is normally 1. */
UCLASS(meta = (DisplayName = "Visit"))
class QUESTFRAMEWORK_API UQuestObjective_Visit : public UQuestObjective
{
	GENERATED_BODY()

public:
	/** The place to reach, as the game reports it in the event's ContextId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FName ContextId;

	/** The event's Amount for a LocationReached event with this context id, else 0. */
	virtual int32 EvaluateEvent(const FQuestEvent& Event) const override;

	/** Also needs a context id. */
	virtual bool IsObjectiveValid(FText& OutError) const override;
};
