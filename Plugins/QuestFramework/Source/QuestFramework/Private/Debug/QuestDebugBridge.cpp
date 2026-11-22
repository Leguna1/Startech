#include "Debug/QuestDebugBridge.h"

#include "QuestFrameworkLog.h"
#include "QuestTypes.h"

int32 UQuestDebugBridge::GetItemCount(const FGameplayTag& ItemTag) const
{
	const FQuestDebugItem* const Item = Items.FindByPredicate([&ItemTag](const FQuestDebugItem& Each)
	{
		return Each.ItemTag == ItemTag;
	});
	return Item ? Item->Count : 0;
}

bool UQuestDebugBridge::RemoveItems(const FGameplayTag& ItemTag, const int32 Count)
{
	if (GetItemCount(ItemTag) < Count)
	{
		return false;
	}
	AddItems(ItemTag, -Count);
	UE_LOG(LogQuestFramework, Display, TEXT("[Test bridge] Removed %d x %s."), Count, *ItemTag.ToString());
	return true;
}

void UQuestDebugBridge::GrantReward(const FQuestReward& Reward)
{
	UE_LOG(LogQuestFramework, Display, TEXT("[Test bridge] Granted %d x %s."), Reward.Amount, *Reward.RewardTag.ToString());
}

int32 UQuestDebugBridge::AddItems(const FGameplayTag& ItemTag, const int32 Delta)
{
	FQuestDebugItem* Item = Items.FindByPredicate([&ItemTag](const FQuestDebugItem& Each)
	{
		return Each.ItemTag == ItemTag;
	});
	if (!Item)
	{
		Item = &Items.AddDefaulted_GetRef();
		Item->ItemTag = ItemTag;
	}
	Item->Count = FMath::Max(Item->Count + Delta, 0);
	return Item->Count;
}
