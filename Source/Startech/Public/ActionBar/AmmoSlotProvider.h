#pragma once

#include "CoreMinimal.h"
#include "ActionBar/ActionSlotProvider.h"
#include "WeaponGroup.h"
#include "AmmoSlotProvider.generated.h"

class UAmmoData;
class UTargetRangeComponent;
class UWeaponSystemComponent;

/**
 * Ammo slots: selected when the ammo is its weapon group's selection; Count is the whole rounds held (-1, infinite,
 * for the free ammo LL1 or with infinite ammo on), low below the threshold, empty at 0; out of range while the target
 * is beyond the group's range. Pressing selects the ammo for its group and makes the group active; the wheel cycles within the active group. Follows the
 * weapon system's ammo and selection events and the target range component's group range events.
 */
UCLASS()
class STARTECH_API UAmmoSlotProvider : public UActionSlotProvider
{
	GENERATED_BODY()

public:
	/** Binds to Ship's weapon system and target range component. */
	virtual void Bind(AStartechShipPawn& Ship) override;

	/** Unbinds from them. */
	virtual void Unbind() override;

	/** The ammo slot's state from the weapon system and the range component. */
	virtual FActionSlotState GetState(const FActionBarSlot& Slot, int32 LowThreshold) const override;

	/**
	 * The ammo's icon texture if it has one (UTradeableData::Icon), otherwise drawn: bars for lasers, chevrons for
	 * shield drain, as many as the ammo's IconTier.
	 */
	virtual FActionSlotVisual GetVisual(const FActionBarSlot& Slot) const override;

	/** Selects the slot's ammo for its group and makes the group active. */
	virtual bool Activate(const FActionBarSlot& Slot) override;

	/** Cycles the active group's selection through the ammo slots, in bar order. */
	virtual bool Cycle(const TArray<FActionBarSlot>& Slots, int32 Direction) override;

	/** The ammo in Slot (UStartechSettings::ItemCatalog), or null. */
	static UAmmoData* GetAmmo(const FActionBarSlot& Slot);

private:
	/** Weapon system: a count changed. */
	UFUNCTION()
	void HandleAmmoChanged(UAmmoData* Ammo, float NewCount);

	/** Weapon system: a group's selection changed. */
	UFUNCTION()
	void HandleGroupAmmoSelected(EWeaponGroup Group, UAmmoData* Ammo);

	/** Weapon system: the active group changed. */
	UFUNCTION()
	void HandleActiveGroupChanged(EWeaponGroup Group);

	/** Range component: a group went in or out of range. */
	UFUNCTION()
	void HandleGroupRangeChanged(EWeaponGroup Group, bool bOutOfRange);

	/** The ship's weapon system. */
	TWeakObjectPtr<UWeaponSystemComponent> Weapons;

	/** The ship's target range component. */
	TWeakObjectPtr<UTargetRangeComponent> Range;
};
