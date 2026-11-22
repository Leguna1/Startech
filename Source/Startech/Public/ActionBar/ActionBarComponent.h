#pragma once

#include "CoreMinimal.h"
#include "ActionBar/ActionBarTypes.h"
#include "Components/ActorComponent.h"
#include "ActionBarComponent.generated.h"

class UActionBarLayout;
class UActionSlotProvider;
class UEnhancedInputComponent;

/**
 * The player ship's action bar: the layout (UStartechSettings::ActionBarLayout, DA_ActionBarLayout), one provider per
 * slot kind in it, and the slot keys. Pressing a slot's input action activates the slot through its provider; the
 * mouse wheel cycles the active choice. Both do nothing while the station hub or the pause menu is open. The HUD's
 * action bar reads GetSlotState and repaints on OnSlotsChanged.
 */
UCLASS(ClassGroup = (Startech))
class STARTECH_API UActionBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Never ticks. */
	UActionBarComponent();

	/** Binds every slot's input action (Started) on Input. Called from the ship's SetupPlayerInputComponent. */
	void SetupInput(UEnhancedInputComponent& Input);

	/** The layout, loaded on first use; null when the setting is empty. */
	const UActionBarLayout* GetLayout() const;

	/** Number of slots in the layout. */
	int32 GetSlotCount() const;

	/** The slot at Index; check Index against GetSlotCount. */
	const FActionBarSlot& GetSlot(int32 Index) const;

	/** The state of the slot at Index now; LowThreshold is the count below which it counts as low. */
	FActionSlotState GetSlotState(int32 Index, int32 LowThreshold) const;

	/** The icon of the slot at Index. */
	FActionSlotVisual GetSlotVisual(int32 Index) const;

	/** The key that presses the slot at Index, from the player's input mappings ("1"); empty when unmapped. */
	FText GetSlotKeyText(int32 Index) const;

	/** Presses the slot at Index, as its key does. False when nothing happened (hub or pause open, no slot). */
	bool ActivateSlot(int32 Index);

	/** Steps the active choice Direction (+1 or -1), as the mouse wheel does. */
	void CycleActive(int32 Direction);

	/** Fires whenever any slot may look different (counts, selection, active group, range). */
	FSimpleMulticastDelegate OnSlotsChanged;

protected:
	/** Creates and binds the providers. */
	virtual void BeginPlay() override;

	/** Unbinds the providers. */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	/** A provider's OnStateChanged: passes it on as OnSlotsChanged. */
	void OnSlotsChangedRelay();

	/** Input callback for the slot at SlotIndex. */
	void HandleSlotPressed(int32 SlotIndex);

	/** The provider for Kind, or null. */
	UActionSlotProvider* FindProvider(EActionSlotKind Kind) const;

	/** True while the hub or the pause menu takes input. */
	bool IsInputBlocked() const;

	/** One provider per slot kind in the layout. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UActionSlotProvider>> Providers;

	/** The kind of each entry in Providers. */
	TArray<EActionSlotKind> ProviderKinds;

	/** The loaded layout. */
	UPROPERTY(Transient)
	mutable TObjectPtr<const UActionBarLayout> Layout;
};
