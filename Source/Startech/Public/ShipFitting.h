#pragma once

#include "CoreMinimal.h"

class AActor;
class UWeaponData;
class UWeaponMountComponent;

// Every change of the weapon fitted to a mount goes through here. Spares come from the ship's UInventoryComponent.
class STARTECH_API FShipFitting
{
public:
	// Fits NewWeapon to Mount, taking one spare of it from the ship's inventory and returning the weapon it
	// replaces, if any, to the spares. False, changing nothing, when Mount isn't the ship's, Mount can't fit
	// NewWeapon, or the ship holds no spare of it.
	static bool FitWeapon(AActor& Ship, UWeaponMountComponent& Mount, UWeaponData* NewWeapon);

	// The next spare Mount can fit, cycling through the spares in inventory order starting after the fitted
	// weapon. Spares of the fitted weapon itself are skipped. Null if there is none.
	static UWeaponData* GetNextFittable(const AActor& Ship, const UWeaponMountComponent& Mount);

	// Loading a save or starting a new game: puts Weapon (null empties it) on Mount without touching the spares.
	// False, changing nothing, when Mount isn't the ship's or can't fit Weapon.
	static bool RestoreWeapon(AActor& Ship, UWeaponMountComponent& Mount, UWeaponData* Weapon);
};
