#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WeaponGroup.h"
#include "WeaponMountComponent.generated.h"

class AController;
class UWeaponData;
class UWeaponSystemComponent;
struct FHitResult;

// A hardpoint. Its world location is the muzzle. Driven each frame by the owner's weapon system.
UCLASS(Abstract, ClassGroup = (Startech))
class STARTECH_API UWeaponMountComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	virtual EWeaponGroup GetGroup() const PURE_VIRTUAL(UWeaponMountComponent::GetGroup, return EWeaponGroup::Laser;);

	// bFiring is false whenever the trigger is up or this mount's group is not the active fire mode.
	virtual void UpdateFiring(bool bFiring, const FVector& AimPoint, float DeltaSeconds) {}

	// True when Weapon is the kind of weapon this mount fires.
	virtual bool CanFit(const UWeaponData* Weapon) const { return false; }

	// Null when the mount is empty.
	virtual UWeaponData* GetFittedWeapon() const { return nullptr; }

	// Null empties the mount. A weapon CanFit rejects is ignored.
	virtual void SetFittedWeapon(UWeaponData* Weapon) {}

	/** Multiplies all damage this mount deals; enemies set it from their sector tier. 1 by default. */
	void SetDamageScale(const float InDamageScale) { DamageScale = FMath::Max(InDamageScale, 0.f); }

	/** The damage multiplier set by SetDamageScale. */
	float GetDamageScale() const { return DamageScale; }

	// The mount's name in the UI, e.g. "Left laser".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

protected:
	virtual void BeginPlay() override;

	UWeaponSystemComponent* GetWeaponSystem() const { return WeaponSystem.Get(); }

	// The owner's controller, for crediting damage. May be null.
	AController* GetInstigatorController() const;

	// Traces from the muzzle toward AimPoint, out to exactly Range, ignoring the owner.
	// OutEnd is the impact point on a hit, otherwise the end of the range.
	bool TraceShot(const FVector& AimPoint, float Range, FHitResult& OutHit, FVector& OutEnd) const;

	// True when HitActor and this mount's owner are on the same team and it isn't Neutral.
	// Friendly hits still stop the shot; they just deal no damage.
	bool IsFriendly(const AActor* HitActor) const;

private:
	TWeakObjectPtr<UWeaponSystemComponent> WeaponSystem;

	/** See SetDamageScale. */
	float DamageScale = 1.f;
};
