#pragma once

#include "CoreMinimal.h"
#include "ActionBar/ActionBarTypes.h"

class UActionSlotProvider;

/** Creates the provider for each kind of action bar slot. A new kind is a new provider class and a case here. */
struct STARTECH_API FActionSlotProviderFactory
{
	/** A new provider for Kind, owned by Outer, or null for a kind with no provider. */
	static UActionSlotProvider* Create(EActionSlotKind Kind, UObject& Outer);
};
