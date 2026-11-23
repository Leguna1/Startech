#include "Controls/ControlsHelpData.h"

#include "StartechSettings.h"

DEFINE_LOG_CATEGORY(LogStartechControls);

const UControlsHelpData* UControlsHelpData::Load()
{
	const UControlsHelpData* const Data = UStartechSettings::Get().ControlsHelp.LoadSynchronous();
	if (!Data)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogStartechControls, Error, TEXT("UStartechSettings::ControlsHelp is not set or missing: the Controls screen is empty."));
		}
	}
	return Data;
}

FText UControlsHelpData::GetKeyLabel(const FKey& Key, const FControlsHelpRow* const Row) const
{
	const auto Find = [&Key](const TArray<FControlsKeyLabel>& Labels) -> const FControlsKeyLabel*
	{
		return Labels.FindByPredicate([&Key](const FControlsKeyLabel& Each) { return Each.Key == Key && !Each.Label.IsEmpty(); });
	};

	if (const FControlsKeyLabel* const RowLabel = Row ? Find(Row->RowKeyLabels) : nullptr)
	{
		return RowLabel->Label;
	}
	if (const FControlsKeyLabel* const Label = Find(KeyLabels))
	{
		return Label->Label;
	}
	return Key.GetDisplayName(false);
}
