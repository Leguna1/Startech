#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatTestAmmoListener.generated.h"

class UAmmoData;

/**
 * Test helper for Startech.Combat.*: records UWeaponSystemComponent::OnAmmoAutoSwitched broadcasts (a dynamic
 * delegate, so it needs a UFUNCTION). Used only by the automation tests.
 */
UCLASS(Transient)
class UCombatTestAmmoListener : public UObject
{
	GENERATED_BODY()

public:
	/** Broadcasts seen. */
	int32 Count = 0;

	/** The last broadcast's ammo. */
	UPROPERTY()
	TObjectPtr<UAmmoData> Exhausted;

	UPROPERTY()
	TObjectPtr<UAmmoData> Replacement;

	/** Records one broadcast. */
	UFUNCTION()
	void HandleAutoSwitched(UAmmoData* InExhausted, UAmmoData* InReplacement)
	{
		++Count;
		Exhausted = InExhausted;
		Replacement = InReplacement;
	}
};
