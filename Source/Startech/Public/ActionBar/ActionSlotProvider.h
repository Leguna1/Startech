#pragma once

#include "CoreMinimal.h"
#include "ActionBar/ActionBarTypes.h"
#include "UObject/Object.h"
#include "ActionSlotProvider.generated.h"

class AStartechShipPawn;

/**
 * What one kind of action bar slot means (Strategy pattern): it fills each slot's FActionSlotState, does what pressing
 * the slot does, and announces when any of its slots' state may have changed. The bar and its widget know no kinds;
 * a new kind (rockets, abilities) is a new subclass and a line in FActionSlotProviderFactory.
 */
UCLASS(Abstract)
class STARTECH_API UActionSlotProvider : public UObject
{
	GENERATED_BODY()

public:
	/** Starts following Ship's events that change this kind's slots. */
	virtual void Bind(AStartechShipPawn& Ship) {}

	/** Stops following them. */
	virtual void Unbind() {}

	/** The state of Slot now. LowThreshold is the count below which a slot counts as low. */
	virtual FActionSlotState GetState(const FActionBarSlot& Slot, int32 LowThreshold) const { return FActionSlotState(); }

	/** Slot's icon. */
	virtual FActionSlotVisual GetVisual(const FActionBarSlot& Slot) const { return FActionSlotVisual(); }

	/** Does what pressing Slot does. True when it did something. */
	virtual bool Activate(const FActionBarSlot& Slot) { return false; }

	/**
	 * Steps the choice in use Direction (+1 or -1) among Slots of this kind, for the mouse wheel. True when it
	 * handled it.
	 */
	virtual bool Cycle(const TArray<FActionBarSlot>& Slots, int32 Direction) { return false; }

	/** Fires whenever a slot of this kind may look different. */
	FSimpleMulticastDelegate OnStateChanged;
};
