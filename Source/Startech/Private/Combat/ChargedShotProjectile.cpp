#include "Combat/ChargedShotProjectile.h"

#include "Combat/ChargedShotSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "StartechSettings.h"

AChargedShotProjectile::AChargedShotProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AChargedShotProjectile::Launch(const FChargedShotLaunch& InLaunch)
{
	ClearFlight();

	LaunchData = InLaunch;
	Velocity = InLaunch.Direction.GetSafeNormal() * InLaunch.Speed;
	Age = 0.f;
	bFlying = true;
	SetActorLocationAndRotation(InLaunch.Start, Velocity.Rotation());
	SetActorTickEnabled(true);

	const FChargedShotEffects& Effects = UStartechSettings::Get().ChargedShot;
	if (UNiagaraSystem* const System = Effects.ProjectileFX.IsNull() ? nullptr : Effects.ProjectileFX.LoadSynchronous())
	{
		Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Root, NAME_None, FVector::ZeroVector,
			FRotator::ZeroRotator, FVector(Effects.ProjectileFXScale), EAttachLocation::SnapToTarget, false,
			ENCPoolMethod::None);
	}
}

void AChargedShotProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFlying)
	{
		return;
	}

	Age += DeltaSeconds;
	const FVector From = GetActorLocation();
	const FVector To = From + Velocity * DeltaSeconds;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChargedShotSweep), false, this);
	if (AActor* const Shooter = LaunchData.Shooter.Get())
	{
		Params.AddIgnoredActor(Shooter);
	}

	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(LaunchData.Radius), Params))
	{
		SetActorLocation(Hit.Location);
		End(&Hit);
		return;
	}

	SetActorLocation(To);
	if (Age >= LaunchData.Lifetime)
	{
		End(nullptr);
	}
}

void AChargedShotProjectile::End(const FHitResult* const Hit)
{
	AActor* const HitActor = Hit ? Hit->GetActor() : nullptr;
	bool bDamaged = false;

	if (HitActor)
	{
		const ITeamInterface* const HitTeam = Cast<ITeamInterface>(HitActor);
		const bool bFriendly = HitTeam && LaunchData.Team != ETeam::Neutral && HitTeam->GetTeam() == LaunchData.Team;
		if (!bFriendly && LaunchData.Damage > 0.f)
		{
			// Credited to the shooter (the combat log reads the instigator), even if it has died since.
			AActor* const Causer = LaunchData.Shooter.IsValid() ? LaunchData.Shooter.Get() : this;
			bDamaged = UGameplayStatics::ApplyDamage(HitActor, LaunchData.Damage, nullptr, Causer,
				UDamageType::StaticClass()) > 0.f;
		}
	}

	if (Hit)
	{
		const FChargedShotEffects& Effects = UStartechSettings::Get().ChargedShot;
		if (UNiagaraSystem* const System = Effects.ImpactFX.IsNull() ? nullptr : Effects.ImpactFX.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Hit->Location, FRotator::ZeroRotator,
				FVector(Effects.ImpactFXScale));
		}
		if (USoundBase* const Sound = Effects.ImpactSound.IsNull() ? nullptr : Effects.ImpactSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit->Location);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Charge] shot from %s ended after %.2f s: %s"), *GetNameSafe(LaunchData.Shooter.Get()), Age,
		HitActor ? *FString::Printf(TEXT("hit %s%s"), *HitActor->GetName(), bDamaged ? TEXT(" (damaged)") : TEXT("")) : TEXT("expired"));
	ClearFlight();
	UChargedShotSubsystem::OnAnyChargedShotEnded.Broadcast(*this, HitActor, bDamaged);

	if (UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(this))
	{
		Shots->Release(*this);
	}
}

void AChargedShotProjectile::ClearFlight()
{
	bFlying = false;
	Velocity = FVector::ZeroVector;
	if (UNiagaraComponent* const Effect = Trail.Get())
	{
		Effect->DestroyComponent();
	}
	Trail.Reset();
}

void AChargedShotProjectile::OnAcquiredFromPool_Implementation()
{
	ClearFlight();
	Age = 0.f;
}

void AChargedShotProjectile::OnReleasedToPool_Implementation()
{
	ClearFlight();
	LaunchData = FChargedShotLaunch();
}
