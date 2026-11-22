#pragma once

#include "CoreMinimal.h"
#include "WeaponGroup.generated.h"

class UAmmoData;

// Which fire mode a weapon or ammo type belongs to.
UENUM(BlueprintType)
enum class EWeaponGroup : uint8
{
	Laser,
	ShieldDrain
};

/** The ammo selected for one weapon group. Each group keeps its own selection; one group is the active one. */
USTRUCT()
struct STARTECH_API FWeaponGroupSelection
{
	GENERATED_BODY()

	/** The group. */
	UPROPERTY()
	EWeaponGroup Group = EWeaponGroup::Laser;

	/** Its selected ammo, or null for none. */
	UPROPERTY()
	TObjectPtr<UAmmoData> Ammo;
};

/** Helpers over EWeaponGroup. */
struct STARTECH_API FWeaponGroups
{
	/** Every weapon group, in enum order. */
	static TArray<EWeaponGroup> All();
};
