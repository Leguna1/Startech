#pragma once

#include "CoreMinimal.h"
#include "WeaponMountComponent.h"
#include "ShieldDrainMountComponent.generated.h"

class AActor;
class UAmmoData;
class UShieldDrainMountComponent;
class UShieldDrainWeaponData;

// Every beam delegate passes the mount that sent it first.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDrainBeamStarted, UShieldDrainMountComponent*, Mount, UAmmoData*, Ammo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDrainBeamUpdated, UShieldDrainMountComponent*, Mount, FVector, Start, FVector, End, bool, bHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDrainBeamStopped, UShieldDrainMountComponent*, Mount);

// A continuous beam that drains shields only, paid for with drain ammo over time.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UShieldDrainMountComponent : public UWeaponMountComponent
{
	GENERATED_BODY()

public:
	virtual EWeaponGroup GetGroup() const override { return EWeaponGroup::ShieldDrain; }
	virtual void UpdateFiring(bool bFiring, const FVector& AimPoint, float DeltaSeconds) override;

	bool IsBeamActive() const { return bBeamActive; }
	const UShieldDrainWeaponData* GetWeapon() const { return Weapon; }

	// Changing the weapon while the beam runs stops the beam; it starts again next frame if still firing.
	virtual bool CanFit(const UWeaponData* InWeapon) const override;
	virtual UWeaponData* GetFittedWeapon() const override;
	virtual void SetFittedWeapon(UWeaponData* InWeapon) override;

	// Ammo is the selected drain ammo; it decides the beam's look.
	// If that ammo changes while the beam runs, the beam stops and starts again with the new one.
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnDrainBeamStarted OnBeamStarted;

	// Every frame the beam is on.
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnDrainBeamUpdated OnBeamUpdated;

	// Trigger released, another group's ammo selected, or out of ammo.
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnDrainBeamStopped OnBeamStopped;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TObjectPtr<UShieldDrainWeaponData> Weapon;

private:
	// Spends Ammo for this frame of beam. False when out of it.
	bool PayForBeam(const UAmmoData& Ammo, float DeltaSeconds);

	// Applies this frame's drain to HitActor and, if Ammo steals shields, gives the owner its share.
	void DrainTarget(AActor& HitActor, const UAmmoData& Ammo, float DeltaSeconds) const;

	void StopBeam();

	// Fraction of a round already paid for and not yet used.
	float AmmoCredit = 0.f;

	// The ammo AmmoCredit was paid in.
	TWeakObjectPtr<const UAmmoData> CreditAmmo;

	bool bBeamActive = false;

	// The ammo the running beam started with.
	TWeakObjectPtr<UAmmoData> BeamAmmo;
};
