#include "LaserMountComponent.h"

#include "AmmoData.h"
#include "Combat/CombatBalance.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "LaserWeaponData.h"
#include "WeaponSystemComponent.h"

FOnAnyLaserShot ULaserMountComponent::OnAnyShotFired;

namespace
{
	// Shared by every laser mount so ids never collide between mounts.
	int32 LastShotId = 0;

	int32 MakeShotId()
	{
		LastShotId = LastShotId == MAX_int32 ? 1 : LastShotId + 1;
		return LastShotId;
	}
}

ULaserMountComponent::ULaserMountComponent()
{
	// Ticks only while shots are in flight.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool ULaserMountComponent::CanFit(const UWeaponData* const InWeapon) const
{
	return InWeapon && InWeapon->IsA<ULaserWeaponData>();
}

UWeaponData* ULaserMountComponent::GetFittedWeapon() const
{
	return Weapon;
}

void ULaserMountComponent::SetFittedWeapon(UWeaponData* const InWeapon)
{
	if (!InWeapon || CanFit(InWeapon))
	{
		SetWeapon(Cast<ULaserWeaponData>(InWeapon));
	}
}

void ULaserMountComponent::UpdateFiring(const bool bFiring, const FVector& AimPoint, const float DeltaSeconds)
{
	Cooldown -= DeltaSeconds;

	if (!bFiring || !Weapon || Weapon->FireRate <= 0.f || Cooldown > 0.f)
	{
		// Time spent idle can't be banked for a burst later.
		Cooldown = FMath::Max(Cooldown, 0.f);
		return;
	}

	UWeaponSystemComponent* const Weapons = GetWeaponSystem();
	if (!Weapons)
	{
		return;
	}

	UAmmoData* const Ammo = Weapons->GetSelectedAmmo();
	if (!Ammo || Ammo->Group != EWeaponGroup::Laser)
	{
		return;
	}

	if (!Weapons->ConsumeAmmo(Ammo, 1.f))
	{
		return;
	}

	Fire(AimPoint, Ammo);

	// Carry over the remainder so the fire rate holds at low frame rates.
	Cooldown = FMath::Max(Cooldown + 1.f / Weapon->FireRate, 0.f);
}

void ULaserMountComponent::Fire(const FVector& AimPoint, UAmmoData* const Ammo)
{
	// DA_CombatBalance's value for the ammo, else the ammo's own.
	const float DamageMultiplier = UCombatBalance::GetDamageMultiplier(Ammo);

	FHitResult Hit;
	FVector End;
	const bool bHit = TraceShot(AimPoint, Weapon->Range, Hit, End);
	const FVector Start = GetComponentLocation();

	FShot Shot;
	Shot.Id = MakeShotId();
	Shot.Start = Start;
	Shot.EndPoint = End;
	Shot.Location = Start;
	Shot.Rotation = (End - Start).Rotation();
	Shot.InstigatorController = GetInstigatorController();
	Shot.Ammo = Ammo;
	Shot.Damage = Weapon->Damage * DamageMultiplier * GetDamageScale();
	Shot.Speed = Weapon->BoltSpeed;
	Shot.bHit = bHit;

	if (AActor* const HitActor = Hit.GetActor(); bHit && HitActor)
	{
		Shot.HitActor = HitActor;
		Shot.LocalHitPoint = HitActor->GetActorTransform().InverseTransformPosition(End);
		Shot.bDamageable = !IsFriendly(HitActor);
	}

	Shots.Add(Shot);
	SetComponentTickEnabled(true);

	OnShotFired.Broadcast(this, Shot.Id, Start, Ammo);
	OnAnyShotFired.Broadcast(*this, Shot.HitActor.Get(), Ammo);
}

void ULaserMountComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Backwards, so removing a finished shot never skips one.
	for (int32 Index = Shots.Num() - 1; Index >= 0; --Index)
	{
		// Damage dealt by an earlier shot can end play for this mount, which clears the list.
		if (!Shots.IsValidIndex(Index))
		{
			continue;
		}

		FShot& Shot = Shots[Index];
		const FVector Goal = GetShotGoal(Shot);
		const FVector ToGoal = Goal - Shot.Location;
		const float Step = Shot.Speed * DeltaTime;
		const bool bArrived = Shot.Speed <= 0.f || ToGoal.SizeSquared() <= FMath::Square(Step);

		const FVector NewLocation = bArrived ? Goal : Shot.Location + ToGoal.GetSafeNormal() * Step;
		if (const FVector Moved = NewLocation - Shot.Location; !Moved.IsNearlyZero())
		{
			Shot.Rotation = Moved.Rotation();
		}
		Shot.Location = NewLocation;

		const int32 ShotId = Shot.Id;
		const FRotator Rotation = Shot.Rotation;
		OnShotMoved.Broadcast(this, ShotId, NewLocation, Rotation);

		if (bArrived)
		{
			ResolveShot(Index);
		}
	}

	if (Shots.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

void ULaserMountComponent::ResolveShot(const int32 Index)
{
	const FShot Shot = Shots[Index];
	Shots.RemoveAtSwap(Index);

	AActor* const HitActor = Shot.HitActor.Get();

	// A shot whose hit actor is gone hits nothing. A shot that hit something with no actor still impacts.
	const bool bImpact = Shot.bHit && (HitActor || Shot.HitActor.IsExplicitlyNull());

	if (Shot.bDamageable && HitActor)
	{
		UGameplayStatics::ApplyDamage(HitActor, Shot.Damage,
			Shot.InstigatorController.Get(), GetOwner(), UDamageType::StaticClass());
	}

	OnShotEnded.Broadcast(this, Shot.Id, Shot.Location, bImpact);
}

FVector ULaserMountComponent::GetShotGoal(const FShot& Shot)
{
	const AActor* const HitActor = Shot.HitActor.Get();
	return HitActor ? HitActor->GetActorTransform().TransformPosition(Shot.LocalHitPoint) : Shot.EndPoint;
}

bool ULaserMountComponent::GetShotRotation(const int32 ShotId, FRotator& OutRotation) const
{
	const FShot* const Shot = Shots.FindByPredicate([ShotId](const FShot& Each) { return Each.Id == ShotId; });
	if (!Shot)
	{
		return false;
	}

	OutRotation = Shot->Rotation;
	return true;
}

void ULaserMountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Shots in flight end with the mount, so listeners can clean up after them.
	ClearShots();

	Super::EndPlay(EndPlayReason);
}

void ULaserMountComponent::ClearShots()
{
	Cooldown = 0.f;

	const TArray<FShot> Ended = MoveTemp(Shots);
	Shots.Reset();
	for (const FShot& Shot : Ended)
	{
		OnShotEnded.Broadcast(this, Shot.Id, Shot.Location, false);
	}

	// Ticks only while shots are in flight.
	SetComponentTickEnabled(false);
}
