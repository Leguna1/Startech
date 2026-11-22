#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AmmoStack.h"
#include "WeaponGroup.h"
#include "WeaponSystemComponent.generated.h"

class UAmmoData;
class UWeaponMountComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, UAmmoData*, Ammo, float, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedAmmoChanged, UAmmoData*, NewAmmo);

/** Broadcast when one weapon group's selected ammo changes (null: nothing selected in that group). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroupAmmoSelected, EWeaponGroup, Group, UAmmoData*, Ammo);

/** Broadcast when the active weapon group (the one the trigger fires) changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveWeaponGroupChanged, EWeaponGroup, Group);

/**
 * Broadcast when the selected laser ammo ran out while firing and the weapon switched to the free ammo (LL1) by
 * itself. The HUD shows "LL2 empty, switched to LL1".
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoAutoSwitched, UAmmoData*, Exhausted, UAmmoData*, Replacement);

/** Native: Amount of an ammo type was added (bought, picked up, granted). Not fired when ammo is used. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAmmoAdded, const UAmmoData* /* Ammo */, float /* Amount */);

// One per ship. Owns the ammo and which one is selected, and drives every weapon mount on the owner.
// The selected ammo decides what fires: only mounts of its group, and nothing at all with no selection.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UWeaponSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponSystemComponent();

	// Call once per frame. Mounts outside the selected ammo's group are told to stop.
	void UpdateFiring(bool bTriggerHeld, const FVector& AimPoint, float DeltaSeconds);

	/** Adds Amount of Ammo. Free ammo is never stored, so adding it does nothing. */
	void AddAmmo(UAmmoData* Ammo, float Amount);

	/** The rounds held of Ammo. 0 for the free ammo, which is never stored: use HasAmmo or IsUnlimited. */
	float GetAmmoCount(const UAmmoData* Ammo) const;

	/** True when Ammo never runs out here: the free ammo (LL1), or any ammo with infinite ammo on. */
	bool IsUnlimited(const UAmmoData* Ammo) const;

	/** True when at least one round of Ammo can be fired: unlimited, or one round held. */
	bool HasAmmo(const UAmmoData* Ammo) const;

	/**
	 * The free ammo (UCombatBalance::FreeAmmoTag, LL1): never used up, never stored, and what an empty laser ammo
	 * switches to. Resolved at BeginPlay; tests set it directly.
	 */
	UAmmoData* GetFreeAmmo() const { return FreeAmmo; }
	void SetFreeAmmo(UAmmoData* Ammo);

	/** Every ammo stack held, in the order first acquired. */
	const TArray<FAmmoStack>& GetAmmoStacks() const { return AmmoStacks; }

	/**
	 * Loading a save or starting a new game: replaces every ammo count (types not in NewStacks drop to 0) and fires
	 * OnAmmoChanged for each type whose count changed. A saved count of the free ammo is ignored. Not a gain: OnAmmoAdded stays
	 * silent, so quests count nothing. The selection is left alone; call SelectAmmo after.
	 */
	void RestoreAmmo(const TArray<FAmmoStack>& NewStacks);

	// Removes Amount if the full amount is available. Always succeeds, removing nothing, for unlimited ammo.
	bool ConsumeAmmo(const UAmmoData* Ammo, float Amount);

	bool HasInfiniteAmmo() const { return bInfiniteAmmo; }
	void SetInfiniteAmmo(const bool bInfinite) { bInfiniteAmmo = bInfinite; }

	// Selects Ammo for its weapon group and makes that group the active one (the trigger fires it). Any ammo may be
	// selected, including one the ship has none of (its mounts then just don't fire), or null to fire nothing.
	void SelectAmmo(UAmmoData* Ammo);

	// The active group's selected ammo: what the trigger fires. Null when nothing is selected.
	UAmmoData* GetSelectedAmmo() const { return SelectedAmmo; }

	/**
	 * Selects Ammo for its own group without changing the active group (null does nothing). For restoring a save,
	 * where each group's selection comes back separately.
	 */
	void SelectAmmoForGroup(UAmmoData* Ammo);

	/**
	 * Loading a save or starting a new game: sets every group's selection from PerGroup (one ammo per group; groups
	 * not in it are cleared), then makes Active's group the active one. Groups left without a selection get their
	 * first held ammo.
	 */
	void RestoreSelections(const TArray<UAmmoData*>& PerGroup, UAmmoData* Active);

	/** The ammo selected in Group, or null. Each group keeps its own selection. */
	UAmmoData* GetSelectedAmmoForGroup(EWeaponGroup Group) const;

	/** The active weapon group: the one the trigger fires, with its selected ammo. */
	EWeaponGroup GetActiveGroup() const { return ActiveGroup; }

	/**
	 * The longest Range, in cm, among this ship's mounts of Group that have a weapon fitted (whatever is selected or
	 * held). 0 when the group has no weapon.
	 */
	float GetGroupRange(EWeaponGroup Group) const;

	/**
	 * Moves the active group's selection Direction (+1 or -1) steps along Order, wrapping, to the next ammo of the
	 * active group that HasAmmo. Order is the bar's slot order (other groups' ammo in it is skipped).
	 * Does nothing if there is no other such ammo.
	 */
	void CycleAmmoInActiveGroup(int32 Direction, const TArray<UAmmoData*>& Order);

	/**
	 * The longest Range, in cm, among this ship's mounts that can fire right now: a weapon fitted, in the selected
	 * ammo's group, with that ammo held (or infinite ammo). 0 when nothing can fire.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapons")
	float GetUsableWeaponRange() const;

	// Fired when an ammo count changes: added, used or restored. The action bar's counts follow it.
	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FOnAmmoChanged OnAmmoChanged;

	/** Fires when the active group's selected ammo (GetSelectedAmmo) changes, by selection or by a group switch. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FOnSelectedAmmoChanged OnSelectedAmmoChanged;

	/** Fires when any group's selected ammo changes. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FOnGroupAmmoSelected OnGroupAmmoSelected;

	/** Fires when the active group changes. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FOnActiveWeaponGroupChanged OnActiveGroupChanged;

	/** Fires when an empty laser ammo was swapped for the free ammo while firing. */
	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FOnAmmoAutoSwitched OnAmmoAutoSwitched;

	/** Fires after every AddAmmo that added something, never for use. Native only; the quest bridge listens. */
	FOnAmmoAdded OnAmmoAdded;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	TArray<FAmmoStack> StartingAmmo;

	UPROPERTY(EditDefaultsOnly, Category = "Weapons")
	bool bInfiniteAmmo = false;

private:
	int32 FindStackIndex(const UAmmoData* Ammo) const;

	// While the trigger is held: if the selected laser ammo is down to less than one round, selects the free ammo
	// and fires OnAmmoAutoSwitched. Never touches shield-drain ammo.
	void SelectLaserFallbackIfEmpty();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWeaponMountComponent>> Mounts;

	UPROPERTY(Transient)
	TArray<FAmmoStack> AmmoStacks;

	/** See GetFreeAmmo. */
	UPROPERTY(Transient)
	TObjectPtr<UAmmoData> FreeAmmo;

	/** The active group's selected ammo; mirrors that group's entry in GroupSelections. */
	UPROPERTY(Transient)
	TObjectPtr<UAmmoData> SelectedAmmo;

	/** One selection per weapon group. */
	UPROPERTY(Transient)
	TArray<FWeaponGroupSelection> GroupSelections;

	/** The group the trigger fires. */
	EWeaponGroup ActiveGroup = EWeaponGroup::Laser;

	/** Sets Group's selection to Ammo, firing OnGroupAmmoSelected if it changed. Keeps SelectedAmmo in step. */
	void SetGroupSelection(EWeaponGroup Group, UAmmoData* Ammo);

	/** Sets the active group, firing OnActiveGroupChanged if it changed, and keeps SelectedAmmo in step. */
	void SetActiveGroup(EWeaponGroup Group);

	/** Points SelectedAmmo at the active group's selection, firing OnSelectedAmmoChanged if it changed. */
	void SyncSelectedAmmo();

	/**
	 * Gives every group with no selection its first held ammo (the first stack of that group with rounds), or the free
	 * ammo when it is of that group.
	 */
	void FillEmptyGroupSelections();
};
