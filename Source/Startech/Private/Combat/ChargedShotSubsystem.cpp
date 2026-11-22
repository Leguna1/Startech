#include "Combat/ChargedShotSubsystem.h"

#include "Combat/ChargedShotProjectile.h"
#include "Combat/EnemyChargeComponent.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Engine/World.h"
#include "Population/ActorPoolSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechCharge, Log, All);

FOnAnyChargeStarted UChargedShotSubsystem::OnAnyChargeStarted;
FOnAnyChargedShotFired UChargedShotSubsystem::OnAnyChargedShotFired;
FOnAnyChargedShotEnded UChargedShotSubsystem::OnAnyChargedShotEnded;

UChargedShotSubsystem* UChargedShotSubsystem::Get(const UObject* const WorldContextObject)
{
	const UWorld* const World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UChargedShotSubsystem>() : nullptr;
}

AChargedShotProjectile* UChargedShotSubsystem::Fire(const FChargedShotLaunch& Launch)
{
	UWorld* const World = GetWorld();
	UActorPoolSubsystem* const Pool = World ? World->GetSubsystem<UActorPoolSubsystem>() : nullptr;
	if (!Pool)
	{
		return nullptr;
	}

	AChargedShotProjectile* const Projectile = Cast<AChargedShotProjectile>(
		Pool->Acquire(AChargedShotProjectile::StaticClass(), FTransform(Launch.Direction.Rotation(), Launch.Start)));
	if (!Projectile)
	{
		return nullptr;
	}

	Flying.RemoveAll([](const TWeakObjectPtr<AChargedShotProjectile>& Each) { return !Each.IsValid(); });
	Flying.AddUnique(Projectile);
	Projectile->Launch(Launch);
	UE_LOG(LogStartechCharge, Log, TEXT("[Charge] %s fired: %.0f damage at %.0f cm/s"), *GetNameSafe(Launch.Shooter.Get()),
		Launch.Damage, Launch.Speed);
	OnAnyChargedShotFired.Broadcast(*Projectile);
	return Projectile;
}

void UChargedShotSubsystem::Release(AChargedShotProjectile& Projectile)
{
	const int32 Removed = Flying.RemoveAll([&Projectile](const TWeakObjectPtr<AChargedShotProjectile>& Each)
	{
		return !Each.IsValid() || Each.Get() == &Projectile;
	});
	if (Removed == 0)
	{
		return;
	}

	if (UActorPoolSubsystem* const Pool = GetWorld() ? GetWorld()->GetSubsystem<UActorPoolSubsystem>() : nullptr)
	{
		Pool->Release(&Projectile);
	}
}

int32 UChargedShotSubsystem::GetFlyingCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AChargedShotProjectile>& Each : Flying)
	{
		Count += Each.IsValid() && Each->IsFlying() ? 1 : 0;
	}
	return Count;
}

void UChargedShotSubsystem::RegisterCharge(UEnemyChargeComponent& Charge)
{
	const AActor* const Target = Charge.GetTarget();
	UE_LOG(LogStartechCharge, Log, TEXT("[Charge] %s charging at %s, %.0f m"), *GetNameSafe(Charge.GetOwner()),
		*GetNameSafe(Target), Target && Charge.GetOwner() ? FVector::Dist(Target->GetActorLocation(), Charge.GetOwner()->GetActorLocation()) / 100.f : 0.f);
	Charges.RemoveAll([](const TWeakObjectPtr<UEnemyChargeComponent>& Each) { return !Each.IsValid(); });
	Charges.AddUnique(&Charge);
}

void UChargedShotSubsystem::UnregisterCharge(const UEnemyChargeComponent& Charge)
{
	Charges.RemoveAll([&Charge](const TWeakObjectPtr<UEnemyChargeComponent>& Each)
	{
		return !Each.IsValid() || Each.Get() == &Charge;
	});
}

TArray<const UEnemyChargeComponent*> UChargedShotSubsystem::GetActiveCharges() const
{
	TArray<const UEnemyChargeComponent*> Active;
	for (const TWeakObjectPtr<UEnemyChargeComponent>& Each : Charges)
	{
		if (const UEnemyChargeComponent* const Charge = Each.Get(); Charge && Charge->IsCharging())
		{
			Active.Add(Charge);
		}
	}
	return Active;
}

bool UChargedShotSubsystem::IsCharging(const AActor* const Ship) const
{
	return Ship && Charges.ContainsByPredicate([Ship](const TWeakObjectPtr<UEnemyChargeComponent>& Each)
	{
		const UEnemyChargeComponent* const Charge = Each.Get();
		return Charge && Charge->GetOwner() == Ship && Charge->IsCharging();
	});
}

void UChargedShotSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (USectorDirectorSubsystem* const Director = InWorld.GetSubsystem<USectorDirectorSubsystem>())
	{
		Director->OnSectorTransitionStarted.AddUniqueDynamic(this, &UChargedShotSubsystem::HandleSectorTransitionStarted);
		BoundDirector = Director;
	}
}

void UChargedShotSubsystem::Deinitialize()
{
	if (USectorDirectorSubsystem* const Director = BoundDirector.Get())
	{
		Director->OnSectorTransitionStarted.RemoveDynamic(this, &UChargedShotSubsystem::HandleSectorTransitionStarted);
	}
	BoundDirector.Reset();
	Flying.Reset();
	Charges.Reset();

	Super::Deinitialize();
}

void UChargedShotSubsystem::HandleSectorTransitionStarted(const FName FromSectorId, const FName ToSectorId)
{
	// Shots belong to no sector, so they would otherwise follow the player into the next one.
	const TArray<TWeakObjectPtr<AChargedShotProjectile>> ToRelease = Flying;
	for (const TWeakObjectPtr<AChargedShotProjectile>& Each : ToRelease)
	{
		if (AChargedShotProjectile* const Projectile = Each.Get())
		{
			Release(*Projectile);
		}
	}
	Flying.Reset();
}
