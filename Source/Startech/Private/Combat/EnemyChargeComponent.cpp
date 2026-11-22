#include "Combat/EnemyChargeComponent.h"

#include "Combat/ChargedShotProjectile.h"
#include "Combat/ChargedShotSubsystem.h"
#include "Components/MeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LaserMountComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "StartechEnemyShip.h"
#include "StartechSettings.h"

namespace EnemyCharge
{
	/** Iterations of the flight-time estimate when aiming (each refines where the target will be). */
	constexpr int32 AimIterations = 3;
}

UEnemyChargeComponent::UEnemyChargeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

AStartechEnemyShip* UEnemyChargeComponent::GetShip() const
{
	return Cast<AStartechEnemyShip>(GetOwner());
}

bool UEnemyChargeComponent::BeginCharge(const AActor& InTarget)
{
	const AStartechEnemyShip* const Ship = GetShip();
	if (bCharging || !Ship)
	{
		return false;
	}

	bCharging = true;
	Elapsed = 0.f;
	PulsePhase = 0.f;
	Telegraph = FMath::Max(Ship->GetBehaviour().ChargeTelegraphSeconds, 0.f);
	Target = &InTarget;
	TargetStart = InTarget.GetActorLocation();
	const AStartechShipBase* const TargetShip = Cast<AStartechShipBase>(&InTarget);
	TargetVelocity = TargetShip ? TargetShip->GetShipVelocity() : InTarget.GetVelocity();

	const FChargedShotEffects& Effects = UStartechSettings::Get().ChargedShot;
	if (UMeshComponent* const Mesh = Cast<UMeshComponent>(Ship->GetRootComponent()))
	{
		Glow.Start(*Mesh, Effects.GlowColorParameters, Effects.GlowEmissionParameter);
	}
	if (UNiagaraSystem* const System = Effects.ChargeFX.IsNull() ? nullptr : Effects.ChargeFX.LoadSynchronous())
	{
		const FVector Muzzle = GetMuzzleLocation();
		ChargeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Ship->GetRootComponent(), NAME_None,
			Ship->GetActorTransform().InverseTransformPosition(Muzzle), FRotator::ZeroRotator,
			FVector(Effects.ChargeFXScale), EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None);
	}
	if (USoundBase* const Sound = Effects.ChargeSound.IsNull() ? nullptr : Effects.ChargeSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetMuzzleLocation());
	}

	if (UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(this))
	{
		Shots->RegisterCharge(*this);
	}
	SetComponentTickEnabled(true);

	OnChargeStarted.Broadcast();
	UChargedShotSubsystem::OnAnyChargeStarted.Broadcast(*this);

	if (Telegraph <= 0.f)
	{
		Fire();
	}
	return true;
}

void UEnemyChargeComponent::CancelCharge()
{
	if (bCharging)
	{
		EndCharge();
	}
}

float UEnemyChargeComponent::GetProgress() const
{
	return bCharging && Telegraph > 0.f ? FMath::Clamp(Elapsed / Telegraph, 0.f, 1.f) : 0.f;
}

float UEnemyChargeComponent::GetTimeLeft() const
{
	return bCharging ? FMath::Max(Telegraph - Elapsed, 0.f) : 0.f;
}

FVector UEnemyChargeComponent::GetMuzzleLocation() const
{
	const AStartechShipBase* const Ship = GetShip();
	if (!Ship)
	{
		return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}

	const ULaserMountComponent* const Left = Ship->GetLaserMountLeft();
	const ULaserMountComponent* const Right = Ship->GetLaserMountRight();
	return Left && Right ? (Left->GetComponentLocation() + Right->GetComponentLocation()) * 0.5 : Ship->GetActorLocation();
}

FVector UEnemyChargeComponent::GetAimPoint() const
{
	const AStartechEnemyShip* const Ship = GetShip();
	const float Speed = Ship ? FMath::Max(Ship->GetBehaviour().ChargedShotSpeed, 1.f) : 1.f;
	const FVector Muzzle = GetMuzzleLocation();

	// Where the target's motion at the start of the telegraph takes it by the time the shot would arrive.
	const float Fired = FMath::Max(Elapsed, Telegraph);
	FVector Aim = TargetStart + TargetVelocity * Fired;
	for (int32 Iteration = 0; Iteration < EnemyCharge::AimIterations; ++Iteration)
	{
		const float Flight = FVector::Dist(Muzzle, Aim) / Speed;
		Aim = TargetStart + TargetVelocity * (Fired + Flight);
	}
	return Aim;
}

void UEnemyChargeComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCharging)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Elapsed += DeltaTime;
	if (Elapsed >= Telegraph)
	{
		Fire();
		return;
	}

	// Builds up and pulses faster as the shot nears.
	const FChargedShotEffects& Effects = UStartechSettings::Get().ChargedShot;
	const float Progress = GetProgress();
	PulsePhase += DeltaTime * FMath::Lerp(Effects.GlowPulseHzStart, Effects.GlowPulseHzEnd, Progress);
	const float Pulse = 0.5f - 0.5f * FMath::Cos(PulsePhase * UE_TWO_PI);
	const float Amount = Effects.GlowMaxAmount * FMath::Lerp(0.25f, 1.f, Progress) * FMath::Lerp(0.4f, 1.f, Pulse);
	Glow.Apply(Effects.GlowColor, Amount, 1.f + Effects.GlowEmissionBoost * Amount);
}

void UEnemyChargeComponent::Fire()
{
	AStartechEnemyShip* const Ship = GetShip();
	if (!bCharging || !Ship)
	{
		return;
	}

	Elapsed = Telegraph;
	const FEnemyBehaviourBalance& Behaviour = Ship->GetBehaviour();
	const FVector Muzzle = GetMuzzleLocation();
	const FVector Aim = GetAimPoint();

	FChargedShotLaunch Launch;
	Launch.Shooter = Ship;
	Launch.Team = Ship->GetTeam();
	Launch.Start = Muzzle;
	Launch.Direction = (Aim - Muzzle).GetSafeNormal();
	Launch.Speed = FMath::Max(Behaviour.ChargedShotSpeed, 1.f);
	Launch.Damage = Behaviour.ChargedShotDamage * Ship->GetLifeDamageMultiplier();
	Launch.Radius = Behaviour.ChargedShotRadius;
	Launch.Lifetime = Behaviour.ChargedShotLifetime;

	const FChargedShotEffects& Effects = UStartechSettings::Get().ChargedShot;
	if (USoundBase* const Sound = Effects.FireSound.IsNull() ? nullptr : Effects.FireSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Muzzle);
	}

	EndCharge();

	UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(this);
	AChargedShotProjectile* const Projectile = Shots && !Launch.Direction.IsNearlyZero() ? Shots->Fire(Launch) : nullptr;
	OnChargeFired.Broadcast(Projectile);
}

void UEnemyChargeComponent::EndCharge()
{
	bCharging = false;
	Elapsed = 0.f;
	PulsePhase = 0.f;
	Target.Reset();
	Glow.Restore();
	if (UNiagaraComponent* const Effect = ChargeEffect.Get())
	{
		Effect->DestroyComponent();
	}
	ChargeEffect.Reset();
	if (UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(this))
	{
		Shots->UnregisterCharge(*this);
	}
	SetComponentTickEnabled(false);
}

void UEnemyChargeComponent::OnAcquiredFromPool_Implementation()
{
	CancelCharge();
}

void UEnemyChargeComponent::OnReleasedToPool_Implementation()
{
	CancelCharge();
}

void UEnemyChargeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCharge();
	Super::EndPlay(EndPlayReason);
}
