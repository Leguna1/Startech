#include "WeaponSystemComponent.h"

#include "AmmoData.h"
#include "Combat/CombatBalance.h"
#include "GameFramework/Actor.h"
#include "StartechHUDLog.h"
#include "WeaponData.h"
#include "WeaponMountComponent.h"

UWeaponSystemComponent::UWeaponSystemComponent()
{
	// Driven by the owner through UpdateFiring.
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	Mounts.Reset();
	if (const AActor* const Owner = GetOwner())
	{
		TArray<UWeaponMountComponent*> Found;
		Owner->GetComponents(Found);
		for (UWeaponMountComponent* const Mount : Found)
		{
			Mounts.Add(Mount);
		}
	}

	if (const UCombatBalance* const Balance = UCombatBalance::Get())
	{
		FreeAmmo = Balance->FindFreeAmmo();
	}
	AmmoStacks = StartingAmmo.FilterByPredicate([this](const FAmmoStack& Stack) { return Stack.Ammo != FreeAmmo; });

	// The first ammo carried (else the free ammo) is selected and its group active; the other groups start on their
	// first ammo carried. A save or a new game replaces this right after (UArsenalSaveParticipant).
	const FAmmoStack* const FirstStack = AmmoStacks.FindByPredicate(
		[](const FAmmoStack& Stack) { return Stack.Ammo != nullptr; });
	SelectAmmo(FirstStack ? FirstStack->Ammo.Get() : FreeAmmo.Get());
	FillEmptyGroupSelections();
}

void UWeaponSystemComponent::UpdateFiring(const bool bTriggerHeld, const FVector& AimPoint, const float DeltaSeconds)
{
	// Before the mounts fire, so this tick already uses the fallback.
	if (bTriggerHeld)
	{
		SelectLaserFallbackIfEmpty();
	}

	for (UWeaponMountComponent* const Mount : Mounts)
	{
		if (IsValid(Mount))
		{
			// Only the active group fires, with its selected ammo.
			const bool bActive = SelectedAmmo && Mount->GetGroup() == ActiveGroup;
			Mount->UpdateFiring(bTriggerHeld && bActive, AimPoint, DeltaSeconds);
		}
	}

	// And after: the last round just fired switches now, so the next tick has no gap.
	if (bTriggerHeld)
	{
		SelectLaserFallbackIfEmpty();
	}
}

void UWeaponSystemComponent::SetFreeAmmo(UAmmoData* const Ammo)
{
	FreeAmmo = Ammo;
	AmmoStacks.RemoveAll([Ammo](const FAmmoStack& Stack) { return Ammo && Stack.Ammo == Ammo; });
}

bool UWeaponSystemComponent::IsUnlimited(const UAmmoData* const Ammo) const
{
	return Ammo && (bInfiniteAmmo || Ammo == FreeAmmo);
}

bool UWeaponSystemComponent::HasAmmo(const UAmmoData* const Ammo) const
{
	return Ammo && (IsUnlimited(Ammo) || GetAmmoCount(Ammo) >= 1.f);
}

void UWeaponSystemComponent::SelectAmmo(UAmmoData* const Ammo)
{
	if (!Ammo)
	{
		SetGroupSelection(ActiveGroup, nullptr);
		return;
	}
	SetGroupSelection(Ammo->Group, Ammo);
	SetActiveGroup(Ammo->Group);
}

void UWeaponSystemComponent::SelectAmmoForGroup(UAmmoData* const Ammo)
{
	if (Ammo)
	{
		SetGroupSelection(Ammo->Group, Ammo);
	}
}

void UWeaponSystemComponent::RestoreSelections(const TArray<UAmmoData*>& PerGroup, UAmmoData* const Active)
{
	for (const EWeaponGroup Group : FWeaponGroups::All())
	{
		UAmmoData* const* const Saved = PerGroup.FindByPredicate([Group](const UAmmoData* Ammo) { return Ammo && Ammo->Group == Group; });
		SetGroupSelection(Group, Saved ? *Saved : nullptr);
	}
	if (Active)
	{
		SelectAmmo(Active);
	}
	FillEmptyGroupSelections();
}

UAmmoData* UWeaponSystemComponent::GetSelectedAmmoForGroup(const EWeaponGroup Group) const
{
	const FWeaponGroupSelection* const Selection = GroupSelections.FindByPredicate(
		[Group](const FWeaponGroupSelection& Each) { return Each.Group == Group; });
	return Selection ? Selection->Ammo.Get() : nullptr;
}

void UWeaponSystemComponent::SetGroupSelection(const EWeaponGroup Group, UAmmoData* const Ammo)
{
	FWeaponGroupSelection* Selection = GroupSelections.FindByPredicate(
		[Group](const FWeaponGroupSelection& Each) { return Each.Group == Group; });
	if (!Selection)
	{
		Selection = &GroupSelections.AddDefaulted_GetRef();
		Selection->Group = Group;
	}
	if (Selection->Ammo != Ammo)
	{
		Selection->Ammo = Ammo;
		OnGroupAmmoSelected.Broadcast(Group, Ammo);
	}
	SyncSelectedAmmo();
}

void UWeaponSystemComponent::SetActiveGroup(const EWeaponGroup Group)
{
	if (ActiveGroup != Group)
	{
		ActiveGroup = Group;
		OnActiveGroupChanged.Broadcast(Group);
	}
	SyncSelectedAmmo();
}

void UWeaponSystemComponent::SyncSelectedAmmo()
{
	UAmmoData* const Active = GetSelectedAmmoForGroup(ActiveGroup);
	if (SelectedAmmo == Active)
	{
		return;
	}
	SelectedAmmo = Active;
	OnSelectedAmmoChanged.Broadcast(SelectedAmmo);
}

void UWeaponSystemComponent::FillEmptyGroupSelections()
{
	for (const EWeaponGroup Group : FWeaponGroups::All())
	{
		if (GetSelectedAmmoForGroup(Group))
		{
			continue;
		}
		const FAmmoStack* const First = AmmoStacks.FindByPredicate([Group](const FAmmoStack& Stack)
		{
			return Stack.Ammo && Stack.Ammo->Group == Group && Stack.Count > 0.f;
		});
		if (First)
		{
			SetGroupSelection(Group, First->Ammo);
		}
		else if (FreeAmmo && FreeAmmo->Group == Group)
		{
			SetGroupSelection(Group, FreeAmmo);
		}
	}
}

float UWeaponSystemComponent::GetGroupRange(const EWeaponGroup Group) const
{
	float Range = 0.f;
	for (const UWeaponMountComponent* const Mount : Mounts)
	{
		const UWeaponData* const Weapon = IsValid(Mount) ? Mount->GetFittedWeapon() : nullptr;
		if (Weapon && Mount->GetGroup() == Group)
		{
			Range = FMath::Max(Range, Weapon->Range);
		}
	}
	return Range;
}

void UWeaponSystemComponent::CycleAmmoInActiveGroup(const int32 Direction, const TArray<UAmmoData*>& Order)
{
	const int32 Num = Order.Num();
	if (Num == 0)
	{
		return;
	}
	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 Current = SelectedAmmo ? Order.IndexOfByKey(SelectedAmmo) : INDEX_NONE;
	const int32 Start = Current != INDEX_NONE ? Current : (Step > 0 ? Num - 1 : 0);

	for (int32 Offset = 1; Offset <= Num; ++Offset)
	{
		UAmmoData* const Ammo = Order[((Start + Step * Offset) % Num + Num) % Num];
		if (Ammo && Ammo != SelectedAmmo && Ammo->Group == ActiveGroup && HasAmmo(Ammo))
		{
			SelectAmmo(Ammo);
			return;
		}
	}
}

void UWeaponSystemComponent::SelectLaserFallbackIfEmpty()
{
	UAmmoData* const Exhausted = SelectedAmmo;
	if (!Exhausted || Exhausted->Group != EWeaponGroup::Laser || HasAmmo(Exhausted))
	{
		return;
	}

	// The free ammo never runs out, so the weapon keeps firing.
	if (FreeAmmo && FreeAmmo->Group == EWeaponGroup::Laser)
	{
		SelectAmmo(FreeAmmo);
		OnAmmoAutoSwitched.Broadcast(Exhausted, FreeAmmo);
	}
}

void UWeaponSystemComponent::AddAmmo(UAmmoData* const Ammo, const float Amount)
{
	if (!Ammo || Amount <= 0.f || Ammo == FreeAmmo)
	{
		return;
	}

	int32 Index = FindStackIndex(Ammo);
	if (Index == INDEX_NONE)
	{
		FAmmoStack NewStack;
		NewStack.Ammo = Ammo;
		Index = AmmoStacks.Add(NewStack);
	}

	AmmoStacks[Index].Count += Amount;
	OnAmmoChanged.Broadcast(Ammo, AmmoStacks[Index].Count);
	OnAmmoAdded.Broadcast(Ammo, Amount);
}

void UWeaponSystemComponent::RestoreAmmo(const TArray<FAmmoStack>& NewStacks)
{
	const TArray<FAmmoStack> Before = AmmoStacks;

	AmmoStacks.Reset();
	for (const FAmmoStack& Stack : NewStacks)
	{
		// Saved counts of the free ammo (from before it was free) are dropped, not refunded.
		if (!Stack.Ammo || Stack.Count < 0.f || Stack.Ammo == FreeAmmo)
		{
			continue;
		}
		if (const int32 Index = FindStackIndex(Stack.Ammo); Index != INDEX_NONE)
		{
			AmmoStacks[Index].Count += Stack.Count;
		}
		else
		{
			AmmoStacks.Add(Stack);
		}
	}

	// Every type that changed, including the ones that are gone (now 0).
	for (const FAmmoStack& Old : Before)
	{
		if (Old.Ammo && !FMath::IsNearlyEqual(Old.Count, GetAmmoCount(Old.Ammo)))
		{
			OnAmmoChanged.Broadcast(Old.Ammo, GetAmmoCount(Old.Ammo));
		}
	}
	for (const FAmmoStack& New : AmmoStacks)
	{
		const FAmmoStack* const Old = Before.FindByPredicate([&New](const FAmmoStack& Each) { return Each.Ammo == New.Ammo; });
		if (!Old)
		{
			OnAmmoChanged.Broadcast(New.Ammo, New.Count);
		}
	}
}

float UWeaponSystemComponent::GetAmmoCount(const UAmmoData* const Ammo) const
{
	const int32 Index = FindStackIndex(Ammo);
	return Index != INDEX_NONE ? AmmoStacks[Index].Count : 0.f;
}

bool UWeaponSystemComponent::ConsumeAmmo(const UAmmoData* const Ammo, const float Amount)
{
	if (IsUnlimited(Ammo))
	{
		return true;
	}

	const int32 Index = FindStackIndex(Ammo);
	if (Index == INDEX_NONE || AmmoStacks[Index].Count < Amount)
	{
		return false;
	}

	FAmmoStack& Stack = AmmoStacks[Index];
	Stack.Count -= Amount;
	OnAmmoChanged.Broadcast(Stack.Ammo, Stack.Count);
	return true;
}

float UWeaponSystemComponent::GetUsableWeaponRange() const
{
	if (!HasAmmo(SelectedAmmo))
	{
		return 0.f;
	}

	float Range = 0.f;
	for (const UWeaponMountComponent* const Mount : Mounts)
	{
		const UWeaponData* const Weapon = IsValid(Mount) ? Mount->GetFittedWeapon() : nullptr;
		if (Weapon && Mount->GetGroup() == SelectedAmmo->Group)
		{
			Range = FMath::Max(Range, Weapon->Range);
		}
	}
	return Range;
}

int32 UWeaponSystemComponent::FindStackIndex(const UAmmoData* const Ammo) const
{
	if (!Ammo)
	{
		return INDEX_NONE;
	}

	return AmmoStacks.IndexOfByPredicate([Ammo](const FAmmoStack& Stack) { return Stack.Ammo == Ammo; });
}
