#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "QuestBridge.generated.h"

struct FQuestReward;

/**
 * Asked by the quest subsystem whether a quest may be turned in right now (for example, only at a quest giver).
 * Return false and fill OutReason to refuse. Bind with BindUObject or BindWeakLambda so it holds no strong reference.
 */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FQuestTurnInGate, FName /* QuestId */, FText& /* OutReason */);

/** Native-only interface marker for IQuestInventoryProvider. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UQuestInventoryProvider : public UInterface
{
	GENERATED_BODY()
};

/** The game's inventory as quests see it. Implement on any UObject and register it with UQuestSubsystem::SetInventoryProvider. */
class QUESTFRAMEWORK_API IQuestInventoryProvider
{
	GENERATED_BODY()

public:
	/** How many of the item the player holds now. */
	virtual int32 GetItemCount(const FGameplayTag& ItemTag) const = 0;

	/** Takes Count of the item away, for objectives that consume items on turn-in. Returns false if it could not. */
	virtual bool RemoveItems(const FGameplayTag& ItemTag, int32 Count) = 0;
};

/** Native-only interface marker for IQuestRewardGranter. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UQuestRewardGranter : public UInterface
{
	GENERATED_BODY()
};

/** Gives quest rewards to the player. Implement on any UObject and register it with UQuestSubsystem::SetRewardGranter. */
class QUESTFRAMEWORK_API IQuestRewardGranter
{
	GENERATED_BODY()

public:
	/** Gives the player one reward. The game decides what its tag means. */
	virtual void GrantReward(const FQuestReward& Reward) = 0;
};
