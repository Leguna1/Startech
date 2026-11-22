#pragma once

#include "CoreMinimal.h"
#include "WeaponMountComponent.h"
#include "LaserMountComponent.generated.h"

class AActor;
class AController;
class UAmmoData;
class ULaserMountComponent;
class ULaserWeaponData;

// Every shot delegate passes the mount that sent it first.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLaserShotFired, ULaserMountComponent*, Mount, int32, ShotId, FVector, Start, UAmmoData*, Ammo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLaserShotMoved, ULaserMountComponent*, Mount, int32, ShotId, FVector, Location, FRotator, Rotation);
/**
 * Native, for every laser mount in every world: a shot was fired. AimedAt is the actor its trace hit (the shot will
 * reach it unless it is gone), or null for a miss. The combat log listens.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAnyLaserShot, const ULaserMountComponent& /* Mount */, AActor* /* AimedAt */,
	const UAmmoData* /* Ammo */);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLaserShotEnded, ULaserMountComponent*, Mount, int32, ShotId, FVector, Location, bool, bImpact);

// Fires laser bolts, one round of the selected laser ammo each. A shot is traced the moment it is fired,
// then travels at the weapon's BoltSpeed and deals its damage when it arrives.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API ULaserMountComponent : public UWeaponMountComponent
{
	GENERATED_BODY()

public:
	ULaserMountComponent();

	virtual EWeaponGroup GetGroup() const override { return EWeaponGroup::Laser; }
	virtual void UpdateFiring(bool bFiring, const FVector& AimPoint, float DeltaSeconds) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// For ships that pick their weapon from data at runtime.
	void SetWeapon(ULaserWeaponData* NewWeapon) { Weapon = NewWeapon; }
	const ULaserWeaponData* GetWeapon() const { return Weapon; }

	// Shots already in flight keep the damage and speed they were fired with.
	virtual bool CanFit(const UWeaponData* InWeapon) const override;
	virtual UWeaponData* GetFittedWeapon() const override;
	virtual void SetFittedWeapon(UWeaponData* InWeapon) override;

	/** Ends every shot in flight (reporting each through OnShotEnded, without impact) and clears the cooldown. */
	void ClearShots();

	// The direction a shot of this mount is travelling in. False if the shot isn't in flight from this mount.
	bool GetShotRotation(int32 ShotId, FRotator& OutRotation) const;

	// Shot ids are unique across all laser mounts, so one listener can follow several mounts.
	// Ammo is what the shot was fired with; it decides the shot's look.
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLaserShotFired OnShotFired;

	// Every tick a shot is in flight.
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLaserShotMoved OnShotMoved;

	// bImpact is false when the shot reached the end of its range or its target was gone.
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLaserShotEnded OnShotEnded;

	/** See FOnAnyLaserShot. */
	static FOnAnyLaserShot OnAnyShotFired;

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	TObjectPtr<ULaserWeaponData> Weapon;

private:
	struct FShot
	{
		int32 Id = 0;
		FVector Start = FVector::ZeroVector;

		// Where the trace ended: the impact point on a hit, otherwise the end of the range.
		FVector EndPoint = FVector::ZeroVector;

		// The bolt's current position and direction of travel.
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;

		// Explicitly null when the trace hit no actor; stale once that actor is gone.
		TWeakObjectPtr<AActor> HitActor;

		// The impact point in HitActor's local space, so the bolt follows it as it moves.
		FVector LocalHitPoint = FVector::ZeroVector;

		TWeakObjectPtr<AController> InstigatorController;

		// Kept for the whole flight, whatever the ship switches to meanwhile.
		TWeakObjectPtr<UAmmoData> Ammo;

		float Damage = 0.f;
		float Speed = 0.f;
		bool bHit = false;

		// The hit actor was not friendly when the shot was fired.
		bool bDamageable = false;
	};

	void Fire(const FVector& AimPoint, UAmmoData* Ammo);

	// Removes the shot, applies its damage if it has any to deal, and reports its end.
	void ResolveShot(int32 Index);

	// The hit actor's current position of the impact point while it exists, otherwise the fixed end point.
	static FVector GetShotGoal(const FShot& Shot);

	TArray<FShot> Shots;

	// Seconds until the next shot is allowed.
	float Cooldown = 0.f;
};
