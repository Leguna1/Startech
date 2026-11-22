#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "QuestObjective.generated.h"

class IQuestInventoryProvider;
struct FQuestEvent;

/**
 * One thing a quest asks for (Strategy pattern: one subclass per kind of objective). Objectives are instanced inside
 * their UQuestDefinition and hold no runtime state: progress lives in FQuestRuntimeState, and an objective only says
 * how much an event adds, or reads its progress from the inventory.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories, BlueprintType)
class QUESTFRAMEWORK_API UQuestObjective : public UObject
{
	GENERATED_BODY()

public:
	/** Text shown for the objective. {Current} and {Required} are replaced by the progress. Empty shows "{Current} / {Required}". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Description;

	/** How many are needed to complete it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** How much Event adds to this objective's progress; 0 if it does not count. Event-counted objectives override it. */
	virtual int32 EvaluateEvent(const FQuestEvent& Event) const;

	/** True if progress is read from the inventory (QueryCurrent) instead of counted from events. */
	virtual bool TracksInventory() const;

	/** For an inventory objective: its progress now, read from the game's inventory and capped at RequiredCount. */
	virtual int32 QueryCurrent(const IQuestInventoryProvider& Inventory) const;

	/** True if turning the quest in has to take something from the inventory (see ApplyTurnIn). */
	virtual bool NeedsInventoryOnTurnIn() const;

	/** Called once as the quest is turned in, after every check passed: takes consumed items. Returns false on failure. */
	virtual bool ApplyTurnIn(IQuestInventoryProvider& Inventory) const;

	/** The description with {Current} and {Required} filled in. */
	virtual FText GetProgressText(int32 Current) const;

	/** False with a message when the objective is set up wrong. The base checks RequiredCount. Used by IsDataValid. */
	virtual bool IsObjectiveValid(FText& OutError) const;
};
