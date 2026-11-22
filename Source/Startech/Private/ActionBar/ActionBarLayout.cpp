#include "ActionBar/ActionBarLayout.h"

int32 UActionBarLayout::FindSlotByAction(const UInputAction* const Action) const
{
	if (!Action)
	{
		return INDEX_NONE;
	}
	return Slots.IndexOfByPredicate([Action](const FActionBarSlot& Slot) { return Slot.InputAction == Action; });
}
