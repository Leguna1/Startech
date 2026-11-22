#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestBridge.h"
#include "QuestDebugBridge.generated.h"

/** One item count held by the debug bridge. */
USTRUCT()
struct FQuestDebugItem
{
	GENERATED_BODY()

	/** The item. */
	UPROPERTY()
	FGameplayTag ItemTag;

	/** How many are held. */
	UPROPERTY()
	int32 Count = 0;
};

/**
 * A stand-in game bridge for testing the plugin with console commands alone: a tiny inventory (Quest.Debug.Give)
 * and a reward granter that logs what it would give. Registered with Quest.Debug.UseTestBridge; never used otherwise.
 */
UCLASS()
class QUESTFRAMEWORK_API UQuestDebugBridge : public UObject, public IQuestInventoryProvider, public IQuestRewardGranter
{
	GENERATED_BODY()

public:
	/** The held count of the item. */
	virtual int32 GetItemCount(const FGameplayTag& ItemTag) const override;

	/** Takes Count of the item if that many are held. */
	virtual bool RemoveItems(const FGameplayTag& ItemTag, int32 Count) override;

	/** Logs the reward. */
	virtual void GrantReward(const FQuestReward& Reward) override;

	/** Adds Delta of the item (negative removes, not below 0) and returns the new count. */
	int32 AddItems(const FGameplayTag& ItemTag, int32 Delta);

private:
	/** The held items. */
	UPROPERTY()
	TArray<FQuestDebugItem> Items;
};
