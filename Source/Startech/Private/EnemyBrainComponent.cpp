#include "EnemyBrainComponent.h"

#include "EnemyData.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LaserWeaponData.h"
#include "StartechEnemyShip.h"
#include "WeaponSystemComponent.h"

UEnemyBrainComponent::UEnemyBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* const Owner = GetOwner())
	{
		HomeLocation = Owner->GetActorLocation();

		// The owner's damage event, rather than the health component's, because it says who did it.
		Owner->OnTakeAnyDamage.AddDynamic(this, &UEnemyBrainComponent::HandleOwnerDamaged);
	}
}

void UEnemyBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* const Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UEnemyBrainComponent::HandleOwnerDamaged);
	}

	Super::EndPlay(EndPlayReason);
}

void UEnemyBrainComponent::StopBrain()
{
	bStopped = true;
	SetComponentTickEnabled(false);
}

void UEnemyBrainComponent::ResetBrain(const FVector& Home)
{
	bStopped = false;
	State = EBrainState::Idle;
	HomeLocation = Home;
	WanderPoint = FVector::ZeroVector;
	bHasWanderPoint = false;
	StrafeDirection = 1.f;
	StrafeSwapTimeLeft = 0.f;
	SetComponentTickEnabled(true);
}

void UEnemyBrainComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AStartechEnemyShip* const Ship = GetShip();
	const UEnemyData* const Data = Ship ? Ship->GetEnemyData() : nullptr;
	if (bStopped || !Ship || !Data)
	{
		return;
	}

	// With no living player, combat logic is skipped; the ship still goes home and idles.
	const APawn* const Player = GetLivingPlayer();
	if (!Player && State == EBrainState::Engage)
	{
		SetState(EBrainState::ReturnHome);
	}

	switch (State)
	{
	case EBrainState::Idle:
		TickIdle(*Ship, *Data, Player, DeltaTime);
		break;

	case EBrainState::Engage:
		TickEngage(*Ship, *Data, *Player, DeltaTime);
		break;

	case EBrainState::ReturnHome:
		TickReturnHome(*Ship, *Data, DeltaTime);
		break;
	}
}

void UEnemyBrainComponent::TickIdle(AStartechEnemyShip& Ship, const UEnemyData& Data, const APawn* const Player,
	const float DeltaSeconds)
{
	HoldFire(Ship, DeltaSeconds);

	if (Data.Behavior == EEnemyBehavior::Passive)
	{
		// Only damage from the player wakes a passive enemy (see HandleOwnerDamaged).
		if (!bHasWanderPoint)
		{
			WanderPoint = PickWanderPoint(Data);
			bHasWanderPoint = true;
		}

		if (FlyToward(Ship, Data, WanderPoint, 0.5f))
		{
			bHasWanderPoint = false;
		}
		return;
	}

	HoldPosition(Ship);

	if (Player && FVector::DistSquared(Player->GetActorLocation(), Ship.GetActorLocation()) <= FMath::Square(Data.AggroRange))
	{
		SetState(EBrainState::Engage);
	}
}

void UEnemyBrainComponent::TickEngage(AStartechEnemyShip& Ship, const UEnemyData& Data, const APawn& Player,
	const float DeltaSeconds)
{
	const FVector ShipLocation = Ship.GetActorLocation();
	const FVector PlayerLocation = Player.GetActorLocation();

	if (FVector::DistSquared(PlayerLocation, HomeLocation) > FMath::Square(Data.LeashRange))
	{
		HoldFire(Ship, DeltaSeconds);
		SetState(EBrainState::ReturnHome);
		return;
	}

	Ship.SetFacingLocation(PlayerLocation, Data.TurnRate);
	Ship.SetTurnInput(0.f);

	// Close in or back off to PreferredRange, with a dead band so it doesn't jitter at the edge.
	const float Distance = FVector::Dist(ShipLocation, PlayerLocation);
	// The ship's preferred range and laser are this life's (DA_CombatBalance), not EnemyData's.
	const float PreferredRange = Ship.GetPreferredRange();
	const float Band = PreferredRange * RangeTolerance;
	const float Throttle = Distance > PreferredRange + Band ? 1.f
		: Distance < PreferredRange - Band ? -1.f
		: 0.f;
	Ship.SetThrottleInput(Throttle);

	Ship.SetVerticalInput(GetVerticalIntent(PlayerLocation.Z - ShipLocation.Z));

	if (Data.Behavior == EEnemyBehavior::Ranged)
	{
		StrafeSwapTimeLeft -= DeltaSeconds;
		if (StrafeSwapTimeLeft <= 0.f)
		{
			StrafeDirection = -StrafeDirection;
			StrafeSwapTimeLeft = PickStrafeSwapTime();
		}
		Ship.SetStrafeInput(StrafeDirection);
	}
	else
	{
		Ship.SetStrafeInput(0.f);
	}

	// Fire only with the player in laser range and inside the cone off the nose.
	const ULaserWeaponData* const Laser = Ship.GetLaserWeapon();
	const bool bInRange = Laser && Distance <= Laser->Range;
	const FVector ToPlayer = (PlayerLocation - ShipLocation).GetSafeNormal();
	const float ConeCos = FMath::Cos(FMath::DegreesToRadians(Data.FiringConeHalfAngle));
	const bool bInCone = FVector::DotProduct(Ship.GetActorForwardVector(), ToPlayer) >= ConeCos;

	if (UWeaponSystemComponent* const Weapons = Ship.GetWeaponSystem())
	{
		Weapons->UpdateFiring(bInRange && bInCone, PlayerLocation, DeltaSeconds);
	}
}

void UEnemyBrainComponent::TickReturnHome(AStartechEnemyShip& Ship, const UEnemyData& Data, const float DeltaSeconds)
{
	HoldFire(Ship, DeltaSeconds);

	// Damage on the way back re-enters Engage (see HandleOwnerDamaged).
	if (FlyToward(Ship, Data, HomeLocation, 1.f))
	{
		SetState(EBrainState::Idle);
	}
}

void UEnemyBrainComponent::SetState(const EBrainState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;
	bHasWanderPoint = false;

	if (State == EBrainState::Engage)
	{
		StrafeDirection = FMath::RandBool() ? 1.f : -1.f;
		StrafeSwapTimeLeft = PickStrafeSwapTime();
	}
}

bool UEnemyBrainComponent::FlyToward(AStartechEnemyShip& Ship, const UEnemyData& Data, const FVector& Destination,
	const float MaxThrottle) const
{
	const FVector ToDestination = Destination - Ship.GetActorLocation();
	if (ToDestination.SizeSquared() <= FMath::Square(ArrivalRadius))
	{
		HoldPosition(Ship);
		return true;
	}

	Ship.SetFacingLocation(Destination, Data.TurnRate);
	Ship.SetTurnInput(0.f);
	Ship.SetStrafeInput(0.f);

	// Throttle up only as the nose comes round, so the ship doesn't orbit a point inside its turn circle.
	const FVector Flat(ToDestination.X, ToDestination.Y, 0.f);
	const FVector LevelForward = FRotator(0.f, Ship.GetActorRotation().Yaw, 0.f).Vector();
	const float Alignment = Flat.SizeSquared() > FMath::Square(ArrivalRadius * 0.5f)
		? FMath::Max(FVector::DotProduct(LevelForward, Flat.GetSafeNormal()), 0.f)
		: 1.f;

	Ship.SetThrottleInput(MaxThrottle * Alignment);
	Ship.SetVerticalInput(GetVerticalIntent(ToDestination.Z));
	return false;
}

void UEnemyBrainComponent::HoldPosition(AStartechEnemyShip& Ship)
{
	Ship.ClearFacingLocation();
	Ship.SetThrottleInput(0.f);
	Ship.SetTurnInput(0.f);
	Ship.SetStrafeInput(0.f);
	Ship.SetVerticalInput(0.f);
}

void UEnemyBrainComponent::HoldFire(AStartechEnemyShip& Ship, const float DeltaSeconds)
{
	if (UWeaponSystemComponent* const Weapons = Ship.GetWeaponSystem())
	{
		Weapons->UpdateFiring(false, Ship.GetActorLocation(), DeltaSeconds);
	}
}

float UEnemyBrainComponent::GetVerticalIntent(const float HeightDifference) const
{
	if (FMath::Abs(HeightDifference) <= HeightDeadzone)
	{
		return 0.f;
	}

	return FMath::Clamp(HeightDifference / HeightFullIntentDistance, -1.f, 1.f);
}

FVector UEnemyBrainComponent::PickWanderPoint(const UEnemyData& Data) const
{
	return HomeLocation + FMath::VRand() * FMath::FRandRange(0.f, Data.WanderRadius);
}

float UEnemyBrainComponent::PickStrafeSwapTime() const
{
	return FMath::FRandRange(StrafeSwapMinTime, FMath::Max(StrafeSwapMinTime, StrafeSwapMaxTime));
}

AStartechEnemyShip* UEnemyBrainComponent::GetShip() const
{
	return Cast<AStartechEnemyShip>(GetOwner());
}

const APawn* UEnemyBrainComponent::GetLivingPlayer() const
{
	const APawn* const Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(Player))
	{
		return nullptr;
	}

	// Docked at a station counts as out of reach, so engaged enemies give up and go home.
	if (const AStartechShipBase* const Ship = Cast<AStartechShipBase>(Player); Ship && Ship->IsDocked())
	{
		return nullptr;
	}

	const UHealthComponent* const Health = Player->FindComponentByClass<UHealthComponent>();
	return Health && Health->IsDead() ? nullptr : Player;
}

void UEnemyBrainComponent::HandleOwnerDamaged(AActor* const DamagedActor, const float Damage,
	const UDamageType* const DamageType, AController* const InstigatedBy, AActor* const DamageCauser)
{
	const APawn* const Player = GetLivingPlayer();
	if (bStopped || !Player || Damage <= 0.f)
	{
		return;
	}

	// Only the player wakes it; stray fire from other enemies does not.
	const bool bByPlayer = DamageCauser == Player || (InstigatedBy && InstigatedBy->GetPawn() == Player);
	if (bByPlayer)
	{
		SetState(EBrainState::Engage);
	}
}
