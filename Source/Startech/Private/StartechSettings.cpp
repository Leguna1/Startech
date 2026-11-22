#include "StartechSettings.h"

#include "TradeableData.h"

UStartechSettings::UStartechSettings()
{
	CategoryName = TEXT("Game");
}

const UStartechSettings& UStartechSettings::Get()
{
	return *GetDefault<UStartechSettings>();
}

UTradeableData* UStartechSettings::FindItem(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	for (const TSoftObjectPtr<UTradeableData>& Entry : ItemCatalog)
	{
		UTradeableData* const Good = Entry.LoadSynchronous();
		if (Good && Good->ItemTag.MatchesTagExact(Tag))
		{
			return Good;
		}
	}
	return nullptr;
}
