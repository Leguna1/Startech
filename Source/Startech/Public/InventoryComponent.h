#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemStack.h"
#include "WeaponStack.h"
#include "InventoryComponent.generated.h"

class UItemData;
class UWeaponData;

/** Broadcast when credits change. Delta is NewTotal minus the previous total (negative when spending). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCreditsChanged, int32, NewTotal, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponsChanged);

/** Broadcast when the cargo hold's used space or capacity changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCargoChanged, int32, Used, int32, Capacity);

/** Broadcast for each item added to the cargo hold, with the quantity actually added. For the pickup feed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, FText, DisplayName, int32, Quantity);

/** Native: one item's count in the cargo hold changed by Delta (positive added, negative removed). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnItemCountChanged, const UItemData* /* Item */, int32 /* Delta */);

// The player's credits, cargo hold and spare weapons.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	/** Credits held. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCredits() const { return Credits; }
	void AddCredits(int32 Amount);

	// Removes Amount if the full amount is available.
	bool SpendCredits(int32 Amount);

	// Returns the amount actually added, limited by free cargo space.
	// bShowInPickupFeed false skips OnItemAdded (the HUD pickup feed), for purchases; every other event still fires.
	int32 AddItem(UItemData* Item, int32 Count, bool bShowInPickupFeed = true);

	// Removes Count if the full amount is held.
	bool RemoveItem(const UItemData* Item, int32 Count);

	int32 GetItemCount(const UItemData* Item) const;
	const TArray<FItemStack>& GetItems() const { return Items; }

	/** The limit on the total count of all items. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCargoCapacity() const { return CargoCapacity; }

	// The total count of all items held.
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCargoUsed() const;
	int32 GetCargoFree() const { return FMath::Max(CargoCapacity - GetCargoUsed(), 0); }

	/**
	 * Sets the cargo capacity (at least 0), for upgrades, and fires OnCargoChanged. A smaller hold never removes
	 * cargo; it only stops new items until there is room again.
	 */
	void SetCargoCapacity(int32 InCapacity);

	// Spare weapons are the ones not fitted to a mount. They take no cargo space.
	void AddWeapon(UWeaponData* Weapon, int32 Count);

	// Removes Count if the full amount is held.
	bool RemoveWeapon(const UWeaponData* Weapon, int32 Count);

	int32 GetWeaponCount(const UWeaponData* Weapon) const;
	const TArray<FWeaponStack>& GetWeapons() const { return Weapons; }

	/**
	 * Loading a save or starting a new game: sets the credits to Amount (at least 0) and fires OnCreditsChanged.
	 * Not a gain or a purchase, so nothing else reacts.
	 */
	void RestoreCredits(int32 Amount);

	/**
	 * Loading a save or starting a new game: replaces the cargo hold's contents (empty stacks dropped, duplicates
	 * merged) and fires OnItemsChanged and OnCargoChanged. Not a pickup: OnItemAdded and OnItemCountChanged stay
	 * silent, so quests count nothing. May exceed the capacity if the save says so; a warning is logged.
	 */
	void RestoreItems(const TArray<FItemStack>& NewItems);

	/** Loading a save or starting a new game: replaces the spare weapons and fires OnWeaponsChanged. */
	void RestoreWeapons(const TArray<FWeaponStack>& NewWeapons);

	/** Fires on every change of the credits, with the new total and the change. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnCreditsChanged OnCreditsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemsChanged OnItemsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnWeaponsChanged OnWeaponsChanged;

	/** Fires whenever items are added or removed, with the used space and the capacity. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnCargoChanged OnCargoChanged;

	/** Fires for each AddItem that added something (unless it was a purchase), after OnCargoChanged. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemAdded OnItemAdded;

	/**
	 * Fires for every AddItem and RemoveItem that changed a count, whatever the cause (pickup, purchase, sale, death,
	 * quest), after the other events. Native only; the quest bridge reports item events from it.
	 */
	FOnItemCountChanged OnItemCountChanged;

protected:
	// The limit on the total count of all items.
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 CargoCapacity = 100;

private:
	/** Logs and broadcasts OnCreditsChanged for a change of Delta. */
	void NotifyCreditsChanged(int32 Delta);

	/** Logs and broadcasts OnCargoChanged with the current used space and capacity. */
	void NotifyCargoChanged();

	int32 FindStackIndex(const UItemData* Item) const;
	int32 FindWeaponIndex(const UWeaponData* Weapon) const;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	int32 Credits = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FItemStack> Items;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FWeaponStack> Weapons;
};
