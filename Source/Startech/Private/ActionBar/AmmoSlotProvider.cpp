#include "ActionBar/AmmoSlotProvider.h"

#include "AmmoData.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "TargetRangeComponent.h"
#include "WeaponSystemComponent.h"

UAmmoData* UAmmoSlotProvider::GetAmmo(const FActionBarSlot& Slot)
{
	return UStartechSettings::Get().FindItemOfType<UAmmoData>(Slot.ItemTag);
}

void UAmmoSlotProvider::Bind(AStartechShipPawn& Ship)
{
	Unbind();
	Weapons = Ship.GetWeaponSystem();
	Range = Ship.GetTargetRange();

	if (UWeaponSystemComponent* const WeaponSystem = Weapons.Get())
	{
		WeaponSystem->OnAmmoChanged.AddUniqueDynamic(this, &UAmmoSlotProvider::HandleAmmoChanged);
		WeaponSystem->OnGroupAmmoSelected.AddUniqueDynamic(this, &UAmmoSlotProvider::HandleGroupAmmoSelected);
		WeaponSystem->OnActiveGroupChanged.AddUniqueDynamic(this, &UAmmoSlotProvider::HandleActiveGroupChanged);
	}
	if (UTargetRangeComponent* const TargetRange = Range.Get())
	{
		TargetRange->OnGroupRangeChanged.AddUniqueDynamic(this, &UAmmoSlotProvider::HandleGroupRangeChanged);
	}
}

void UAmmoSlotProvider::Unbind()
{
	if (UWeaponSystemComponent* const WeaponSystem = Weapons.Get())
	{
		WeaponSystem->OnAmmoChanged.RemoveDynamic(this, &UAmmoSlotProvider::HandleAmmoChanged);
		WeaponSystem->OnGroupAmmoSelected.RemoveDynamic(this, &UAmmoSlotProvider::HandleGroupAmmoSelected);
		WeaponSystem->OnActiveGroupChanged.RemoveDynamic(this, &UAmmoSlotProvider::HandleActiveGroupChanged);
	}
	if (UTargetRangeComponent* const TargetRange = Range.Get())
	{
		TargetRange->OnGroupRangeChanged.RemoveDynamic(this, &UAmmoSlotProvider::HandleGroupRangeChanged);
	}
	Weapons.Reset();
	Range.Reset();
}

FActionSlotState UAmmoSlotProvider::GetState(const FActionBarSlot& Slot, const int32 LowThreshold) const
{
	FActionSlotState State;
	const UAmmoData* const Ammo = GetAmmo(Slot);
	const UWeaponSystemComponent* const WeaponSystem = Weapons.Get();
	if (!Ammo || !WeaponSystem)
	{
		State.bDisabled = true;
		return State;
	}

	State.bSelected = WeaponSystem->GetSelectedAmmoForGroup(Ammo->Group) == Ammo;
	State.bInActiveGroup = WeaponSystem->GetActiveGroup() == Ammo->Group;

	// The free ammo (LL1), or everything with infinite ammo on: the bar shows the infinity sign.
	if (WeaponSystem->IsUnlimited(Ammo))
	{
		State.Count = -1;
	}
	else
	{
		State.Count = FMath::FloorToInt32(WeaponSystem->GetAmmoCount(Ammo));
		State.bEmpty = State.Count <= 0;
		State.bLow = !State.bEmpty && State.Count < LowThreshold;
	}

	const UTargetRangeComponent* const TargetRange = Range.Get();
	State.bOutOfRange = TargetRange && TargetRange->IsGroupOutOfRange(Ammo->Group);
	return State;
}

FActionSlotVisual UAmmoSlotProvider::GetVisual(const FActionBarSlot& Slot) const
{
	FActionSlotVisual Visual;
	const UAmmoData* const Ammo = GetAmmo(Slot);
	if (!Ammo)
	{
		return Visual;
	}
	if (Ammo->Icon)
	{
		Visual.Icon = EActionSlotIcon::Texture;
		Visual.Texture = Ammo->Icon;
		return Visual;
	}
	Visual.Icon = Ammo->Group == EWeaponGroup::Laser ? EActionSlotIcon::Bars : EActionSlotIcon::Chevrons;
	Visual.Tier = FMath::Max(Ammo->IconTier, 1);
	return Visual;
}

bool UAmmoSlotProvider::Activate(const FActionBarSlot& Slot)
{
	UAmmoData* const Ammo = GetAmmo(Slot);
	UWeaponSystemComponent* const WeaponSystem = Weapons.Get();
	if (!Ammo || !WeaponSystem)
	{
		return false;
	}
	WeaponSystem->SelectAmmo(Ammo);
	return true;
}

bool UAmmoSlotProvider::Cycle(const TArray<FActionBarSlot>& Slots, const int32 Direction)
{
	UWeaponSystemComponent* const WeaponSystem = Weapons.Get();
	if (!WeaponSystem)
	{
		return false;
	}

	TArray<UAmmoData*> Order;
	for (const FActionBarSlot& Slot : Slots)
	{
		if (Slot.Kind == EActionSlotKind::Ammo)
		{
			if (UAmmoData* const Ammo = GetAmmo(Slot))
			{
				Order.Add(Ammo);
			}
		}
	}
	WeaponSystem->CycleAmmoInActiveGroup(Direction, Order);
	return true;
}

void UAmmoSlotProvider::HandleAmmoChanged(UAmmoData* const Ammo, const float NewCount)
{
	OnStateChanged.Broadcast();
}

void UAmmoSlotProvider::HandleGroupAmmoSelected(const EWeaponGroup Group, UAmmoData* const Ammo)
{
	OnStateChanged.Broadcast();
}

void UAmmoSlotProvider::HandleActiveGroupChanged(const EWeaponGroup Group)
{
	OnStateChanged.Broadcast();
}

void UAmmoSlotProvider::HandleGroupRangeChanged(const EWeaponGroup Group, const bool bOutOfRange)
{
	OnStateChanged.Broadcast();
}
