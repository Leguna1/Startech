#include "ShieldDrainMountComponent.h"

#include "AmmoData.h"
#include "Engine/HitResult.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ShieldDrainDamageType.h"
#include "ShieldDrainWeaponData.h"
#include "WeaponSystemComponent.h"

bool UShieldDrainMountComponent::CanFit(const UWeaponData* const InWeapon) const
{
	return InWeapon && InWeapon->IsA<UShieldDrainWeaponData>();
}

UWeaponData* UShieldDrainMountComponent::GetFittedWeapon() const
{
	return Weapon;
}

void UShieldDrainMountComponent::SetFittedWeapon(UWeaponData* const InWeapon)
{
	if ((InWeapon && !CanFit(InWeapon)) || InWeapon == Weapon)
	{
		return;
	}

	StopBeam();
	Weapon = Cast<UShieldDrainWeaponData>(InWeapon);
}

void UShieldDrainMountComponent::UpdateFiring(const bool bFiring, const FVector& AimPoint, const float DeltaSeconds)
{
	UWeaponSystemComponent* const Weapons = GetWeaponSystem();
	UAmmoData* const Ammo = Weapons ? Weapons->GetSelectedAmmo() : nullptr;
	const bool bDrainAmmo = Ammo && Ammo->Group == EWeaponGroup::ShieldDrain;

	// A different ammo asset means a different look: restart the beam so its effects match.
	if (bBeamActive && BeamAmmo.Get() != Ammo)
	{
		StopBeam();
	}

	if (!bFiring || !Weapon || !bDrainAmmo || !PayForBeam(*Ammo, DeltaSeconds))
	{
		StopBeam();
		return;
	}

	if (!bBeamActive)
	{
		bBeamActive = true;
		BeamAmmo = Ammo;
		OnBeamStarted.Broadcast(this, Ammo);
	}

	FHitResult Hit;
	FVector End;
	const bool bHit = TraceShot(AimPoint, Weapon->Range, Hit, End);

	if (AActor* const HitActor = Hit.GetActor(); bHit && HitActor && !IsFriendly(HitActor))
	{
		DrainTarget(*HitActor, *Ammo, DeltaSeconds);
	}

	OnBeamUpdated.Broadcast(this, GetComponentLocation(), End, bHit);
}

void UShieldDrainMountComponent::DrainTarget(AActor& HitActor, const UAmmoData& Ammo, const float DeltaSeconds) const
{
	// The one place a mount reads health components: shield stealing needs to know how much shield the drain
	// actually removed. The damage itself still goes through ApplyDamage, so invulnerability and enemy aggro
	// work as for any other hit.
	const float TransferRatio = FMath::Clamp(Ammo.ShieldTransferRatio, 0.f, 1.f);
	const UHealthComponent* const TargetHealth = TransferRatio > 0.f ? HitActor.FindComponentByClass<UHealthComponent>() : nullptr;
	const float ShieldBefore = TargetHealth ? TargetHealth->GetShield() : 0.f;

	UGameplayStatics::ApplyDamage(&HitActor, Weapon->ShieldDrainPerSecond * DeltaSeconds * GetDamageScale(),
		GetInstigatorController(), GetOwner(), UShieldDrainDamageType::StaticClass());

	if (!IsValid(TargetHealth))
	{
		return;
	}

	const float Removed = ShieldBefore - TargetHealth->GetShield();
	if (Removed <= 0.f)
	{
		return;
	}

	if (UHealthComponent* const OwnHealth = GetOwner() ? GetOwner()->FindComponentByClass<UHealthComponent>() : nullptr)
	{
		OwnHealth->RestoreShield(Removed * TransferRatio);
	}
}

void UShieldDrainMountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBeam();

	Super::EndPlay(EndPlayReason);
}

bool UShieldDrainMountComponent::PayForBeam(const UAmmoData& Ammo, const float DeltaSeconds)
{
	UWeaponSystemComponent* const Weapons = GetWeaponSystem();
	if (!Weapons)
	{
		return false;
	}

	// A part-used round belongs to the ammo it was bought from; another ammo starts from scratch.
	if (CreditAmmo.Get() != &Ammo)
	{
		CreditAmmo = &Ammo;
		AmmoCredit = 0.f;
	}

	// Rounds are bought whole, ahead of use, and drawn down by AmmoPerSecond.
	AmmoCredit -= Weapon->AmmoPerSecond * DeltaSeconds;
	while (AmmoCredit < 0.f)
	{
		if (!Weapons->ConsumeAmmo(&Ammo, 1.f))
		{
			AmmoCredit = 0.f;
			return false;
		}
		AmmoCredit += 1.f;
	}

	return true;
}

void UShieldDrainMountComponent::StopBeam()
{
	if (bBeamActive)
	{
		bBeamActive = false;
		BeamAmmo = nullptr;
		OnBeamStopped.Broadcast(this);
	}
}
