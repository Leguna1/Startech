#include "EnemyBrainComponent.h"

#include "Combat/CombatBalance.h"
#include "Combat/EnemyChargeComponent.h"
#include "Engine/World.h"
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

	// Damage this enemy deals, for the quiet leash. Native and static, so pooling never leaves it bound twice.
	AnyDamageHandle = UHealthComponent::OnAnyDamageApplied.AddUObject(this, &UEnemyBrainComponent::HandleAnyDamage);
}

void UEnemyBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* const Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UEnemyBrainComponent::HandleOwnerDamaged);
	}
	UHealthComponent::OnAnyDamageApplied.Remove(AnyDamageHandle);
	AnyDamageHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UEnemyBrainComponent::StopBrain()
{
	StopAfterburn(false);
	CancelCharge();
	bRetreating = false;
	bStopped = true;
	SetComponentTickEnabled(false);
}

void UEnemyBrainComponent::ResetBrain(const FVector& Home)
{
	StopAfterburn(false);
	bStopped = false;
	State = EBrainState::Idle;
	HomeLocation = Home;
	WanderPoint = FVector::ZeroVector;
	bHasWanderPoint = false;
	StrafeDirection = 1.f;
	StrafeSwapTimeLeft = 0.f;
	QuietTime = 0.f;
	FarTime = 0.f;
	AfterburnLeft = 0.f;
	AfterburnCooldownLeft = 0.f;
	bRetreating = false;
	ChargeWaitLeft = 0.f;
	CancelCharge();
	LastGiveUpReason = TEXT("");
	SetComponentTickEnabled(true);
}

const TCHAR* UEnemyBrainComponent::GetStateName() const
{
	if (bStopped)
	{
		return TEXT("stopped");
	}

	switch (State)
	{
	case EBrainState::Engage:
		return TEXT("engage");
	case EBrainState::ReturnHome:
		return TEXT("return");
	default:
		return TEXT("idle");
	}
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

	if (!bAfterburning)
	{
		AfterburnCooldownLeft = FMath::Max(AfterburnCooldownLeft - DeltaTime, 0.f);
	}

	// With no living player, combat logic is skipped; the ship still goes home and idles.
	const APawn* const Player = GetLivingPlayer();
	if (!Player && State == EBrainState::Engage)
	{
		GiveUp(*Ship, TEXT("no player"), DeltaTime);
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
	const FEnemyBehaviourBalance& Behaviour = Ship.GetBehaviour();
	const FVector ShipLocation = Ship.GetActorLocation();
	const FVector PlayerLocation = Player.GetActorLocation();
	const float Distance = FVector::Dist(ShipLocation, PlayerLocation);
	// The ship's preferred range and laser are this life's (DA_CombatBalance), not EnemyData's.
	const ULaserWeaponData* const Laser = Ship.GetLaserWeapon();
	const float LaserRange = Laser ? Laser->Range : 0.f;

	if (UpdateLeash(Ship, Behaviour, Distance, LaserRange, PlayerLocation, DeltaSeconds))
	{
		return;
	}

	const float PredictedDistance = GetPredictedDistance(Ship, Player, Distance, 0.f);
	UpdateAfterburn(Ship, Behaviour, Distance, PredictedDistance, LaserRange, DeltaSeconds);

	if (UpdateRetreat(Ship, Data, Behaviour, PlayerLocation, Distance, DeltaSeconds))
	{
		return;
	}

	// A circler out of range heads for a point beside the player, so it arrives circling instead of crossing the
	// player's path. Nothing to shoot at from there.
	if (Behaviour.bCircle && LaserRange > 0.f && Distance > LaserRange)
	{
		HoldFire(Ship, DeltaSeconds);
		const FVector Flank = GetFlankPoint(Ship, PlayerLocation);
		Ship.SetFacingLocation(Flank, Data.TurnRate);
		Ship.SetTurnInput(0.f);
		Ship.SetStrafeInput(0.f);

		// Full throttle once the nose is round, as FlyToward does; braking early if it would overshoot the range.
		const FVector ToFlank = Flank - ShipLocation;
		const FVector Flat(ToFlank.X, ToFlank.Y, 0.f);
		const FVector LevelForward = FRotator(0.f, Ship.GetActorRotation().Yaw, 0.f).Vector();
		Ship.SetThrottleInput(PredictedDistance < Ship.GetPreferredRange() ? -1.f
			: FMath::Max(FVector::DotProduct(LevelForward, Flat.GetSafeNormal()), 0.f));
		Ship.SetVerticalInput(GetVerticalIntent(PlayerLocation.Z - ShipLocation.Z));
		return;
	}

	Ship.SetFacingLocation(PlayerLocation, Data.TurnRate);
	Ship.SetTurnInput(0.f);

	// Close in or back off to PreferredRange, with a dead band so it doesn't jitter at the edge. Backing off as soon as
	// its stopping point is inside the band keeps a fast ship (an afterburner) from overshooting into the player.
	const float PreferredRange = Ship.GetPreferredRange();
	const float Band = PreferredRange * RangeTolerance;
	const float Throttle = PredictedDistance < PreferredRange - Band ? -1.f
		: Distance > PreferredRange + Band ? 1.f
		: 0.f;
	Ship.SetThrottleInput(Throttle);

	Ship.SetVerticalInput(GetVerticalIntent(PlayerLocation.Z - ShipLocation.Z));

	if (Data.Behavior == EEnemyBehavior::Ranged || Behaviour.bCircle)
	{
		UpdateStrafe(Ship, DeltaSeconds);
	}
	else
	{
		Ship.SetStrafeInput(0.f);
	}

	// Fire only with the player in laser range and inside the cone off the nose, and not while charging a shot.
	const bool bInRange = LaserRange > 0.f && Distance <= LaserRange;
	if (UpdateCharge(Ship, Behaviour, Player, bInRange, DeltaSeconds))
	{
		HoldFire(Ship, DeltaSeconds);
		return;
	}

	const FVector ToPlayer = (PlayerLocation - ShipLocation).GetSafeNormal();
	const float ConeCos = FMath::Cos(FMath::DegreesToRadians(Data.FiringConeHalfAngle));
	const bool bInCone = FVector::DotProduct(Ship.GetActorForwardVector(), ToPlayer) >= ConeCos;

	if (UWeaponSystemComponent* const Weapons = Ship.GetWeaponSystem())
	{
		Weapons->UpdateFiring(bInRange && bInCone, PlayerLocation, DeltaSeconds);
	}
}

bool UEnemyBrainComponent::UpdateLeash(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour,
	const float Distance, const float LaserRange, const FVector& PlayerLocation, const float DeltaSeconds)
{
	if (FVector::DistSquared(PlayerLocation, HomeLocation) > FMath::Square(Ship.GetHomeLeashRange()))
	{
		GiveUp(Ship, TEXT("home"), DeltaSeconds);
		return true;
	}

	QuietTime += DeltaSeconds;
	if (Behaviour.LeashQuietSeconds > 0.f && QuietTime >= Behaviour.LeashQuietSeconds)
	{
		GiveUp(Ship, TEXT("quiet"), DeltaSeconds);
		return true;
	}

	if (Behaviour.LeashFarRangeFactor > 0.f && LaserRange > 0.f)
	{
		FarTime = Distance > LaserRange * Behaviour.LeashFarRangeFactor ? FarTime + DeltaSeconds : 0.f;
		if (FarTime >= Behaviour.LeashFarSeconds)
		{
			GiveUp(Ship, TEXT("far"), DeltaSeconds);
			return true;
		}
	}
	return false;
}

float UEnemyBrainComponent::GetPredictedDistance(const AStartechEnemyShip& Ship, const APawn& Player, const float Distance,
	const float SlowToSpeed)
{
	const AStartechShipBase* const PlayerShip = Cast<AStartechShipBase>(&Player);
	const FVector PlayerVelocity = PlayerShip ? PlayerShip->GetShipVelocity() : FVector::ZeroVector;
	const FVector ToPlayer = (Player.GetActorLocation() - Ship.GetActorLocation()).GetSafeNormal();
	const float Closing = FVector::DotProduct(Ship.GetShipVelocity() - PlayerVelocity, ToPlayer);
	// The closing speed it keeps once slowed to SlowToSpeed (straight at the player).
	const float ClosingAfter = FMath::Max(SlowToSpeed - FVector::DotProduct(PlayerVelocity, ToPlayer), 0.f);
	if (Closing <= ClosingAfter)
	{
		return Distance;
	}
	return Distance - (FMath::Square(Closing) - FMath::Square(ClosingAfter)) / (2.f * FMath::Max(Ship.GetAcceleration(), 1.f));
}

void UEnemyBrainComponent::UpdateAfterburn(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour,
	const float Distance, const float PredictedDistance, const float LaserRange, const float DeltaSeconds)
{
	if (bAfterburning)
	{
		AfterburnLeft -= DeltaSeconds;
		// Ends on reaching its preferred range: deep enough inside its gun that a player backing off a little faster than
		// its normal speed takes seconds to get out again. It never runs into the player: the range band's braking (on
		// the predicted stopping point) acts during a burn too.
		if (AfterburnLeft <= 0.f || Distance <= Ship.GetPreferredRange())
		{
			StopAfterburn(true);
		}
		return;
	}

	if (Behaviour.AfterburnMultiplier > 1.f && AfterburnCooldownLeft <= 0.f && LaserRange > 0.f
		&& Distance > LaserRange * Behaviour.AfterburnTriggerFactor)
	{
		StartAfterburn(Ship, Behaviour);
	}
}

void UEnemyBrainComponent::StartAfterburn(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour)
{
	bAfterburning = true;
	AfterburnLeft = Behaviour.AfterburnSeconds;
	// The boost scale of the shared flight model: forward top speed and acceleration.
	Ship.SetFlightScales(1.f, Behaviour.AfterburnMultiplier);
}

void UEnemyBrainComponent::StopAfterburn(const bool bStartCooldown)
{
	if (!bAfterburning)
	{
		return;
	}

	bAfterburning = false;
	AfterburnLeft = 0.f;
	if (bStartCooldown)
	{
		if (const AStartechEnemyShip* const Ship = GetShip())
		{
			AfterburnCooldownLeft = Ship->GetBehaviour().AfterburnCooldown;
		}
	}
	if (AStartechEnemyShip* const Ship = GetShip())
	{
		Ship->SetFlightScales(1.f, 1.f);
	}
}

bool UEnemyBrainComponent::UpdateRetreat(AStartechEnemyShip& Ship, const UEnemyData& Data,
	const FEnemyBehaviourBalance& Behaviour, const FVector& PlayerLocation, const float Distance, const float DeltaSeconds)
{
	if (Behaviour.RetreatFraction <= 0.f)
	{
		bRetreating = false;
		return false;
	}

	const float PreferredRange = Ship.GetPreferredRange();
	if (!bRetreating && Distance < PreferredRange * Behaviour.RetreatFraction)
	{
		bRetreating = true;
		CancelCharge();
	}
	else if (bRetreating && Distance >= PreferredRange)
	{
		bRetreating = false;
	}

	if (!bRetreating)
	{
		return false;
	}

	// Nose away from the player and full throttle once it has turned; nothing to shoot at that way.
	HoldFire(Ship, DeltaSeconds);
	const FVector ShipLocation = Ship.GetActorLocation();
	const FVector Away = FVector(ShipLocation.X - PlayerLocation.X, ShipLocation.Y - PlayerLocation.Y, 0.f).GetSafeNormal();
	const FVector Escape = ShipLocation + (Away.IsNearlyZero() ? Ship.GetActorForwardVector() : Away) * PreferredRange;
	Ship.SetFacingLocation(Escape, Data.TurnRate);
	Ship.SetTurnInput(0.f);
	Ship.SetStrafeInput(0.f);
	const FVector LevelForward = FRotator(0.f, Ship.GetActorRotation().Yaw, 0.f).Vector();
	Ship.SetThrottleInput(FMath::Max(FVector::DotProduct(LevelForward, Away), 0.f));
	Ship.SetVerticalInput(0.f);
	return true;
}

bool UEnemyBrainComponent::UpdateCharge(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour,
	const APawn& Player, const bool bInRange, const float DeltaSeconds)
{
	UEnemyChargeComponent* const Charge = Ship.GetChargedShot();
	if (!Behaviour.bChargedShot || !Charge)
	{
		return false;
	}
	if (Charge->IsCharging())
	{
		return true;
	}

	ChargeWaitLeft -= DeltaSeconds;
	if (ChargeWaitLeft > 0.f || !bInRange || !HasLineOfSight(Ship, Player))
	{
		return false;
	}

	if (!Charge->BeginCharge(Player))
	{
		return false;
	}
	// From one charge's start to the next.
	ChargeWaitLeft = FMath::FRandRange(Behaviour.ChargeIntervalMin, FMath::Max(Behaviour.ChargeIntervalMin, Behaviour.ChargeIntervalMax));
	return Charge->IsCharging();
}

bool UEnemyBrainComponent::HasLineOfSight(const AStartechEnemyShip& Ship, const APawn& Player) const
{
	const UWorld* const World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyLineOfSight), false, &Ship);
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Ship.GetActorLocation(), Player.GetActorLocation(),
		ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == &Player;
}

void UEnemyBrainComponent::CancelCharge() const
{
	if (const AStartechEnemyShip* const Ship = GetShip())
	{
		if (UEnemyChargeComponent* const Charge = Ship->GetChargedShot())
		{
			Charge->CancelCharge();
		}
	}
}

void UEnemyBrainComponent::UpdateStrafe(AStartechEnemyShip& Ship, const float DeltaSeconds)
{
	StrafeSwapTimeLeft -= DeltaSeconds;
	if (StrafeSwapTimeLeft <= 0.f)
	{
		StrafeDirection = -StrafeDirection;
		StrafeSwapTimeLeft = PickStrafeSwapTime();
	}
	Ship.SetStrafeInput(StrafeDirection);
}

FVector UEnemyBrainComponent::GetFlankPoint(const AStartechEnemyShip& Ship, const FVector& PlayerLocation) const
{
	const FVector FromPlayer = Ship.GetActorLocation() - PlayerLocation;
	const FVector Flat = FVector(FromPlayer.X, FromPlayer.Y, 0.f).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Flat) * StrafeDirection;

	// Straight at the player from afar (an angled approach would never catch one flying away), turning out to the
	// full flank offset as it reaches its laser range.
	const ULaserWeaponData* const Laser = Ship.GetLaserWeapon();
	const float Range = Laser ? Laser->Range : 0.f;
	const float Blend = Range > 0.f ? FMath::Clamp((FlankBlendStart * Range - FromPlayer.Size()) / ((FlankBlendStart - 1.f) * Range), 0.f, 1.f) : 1.f;
	return PlayerLocation + Side * Ship.GetPreferredRange() * Blend;
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

void UEnemyBrainComponent::GiveUp(AStartechEnemyShip& Ship, const TCHAR* const Reason, const float DeltaSeconds)
{
	HoldFire(Ship, DeltaSeconds);
	LastGiveUpReason = Reason;
	SetState(EBrainState::ReturnHome);
}

void UEnemyBrainComponent::SetState(const EBrainState NewState)
{
	if (State == NewState)
	{
		return;
	}

	if (State == EBrainState::Engage)
	{
		StopAfterburn(true);
		CancelCharge();
		bRetreating = false;
	}

	State = NewState;
	bHasWanderPoint = false;

	if (State == EBrainState::Engage)
	{
		StrafeDirection = FMath::RandBool() ? 1.f : -1.f;
		StrafeSwapTimeLeft = PickStrafeSwapTime();
		QuietTime = 0.f;
		FarTime = 0.f;
		LastGiveUpReason = TEXT("");
		if (const AStartechEnemyShip* const Ship = GetShip())
		{
			const FEnemyBehaviourBalance& Behaviour = Ship->GetBehaviour();
			ChargeWaitLeft = FMath::FRandRange(Behaviour.ChargeFirstDelayMin,
				FMath::Max(Behaviour.ChargeFirstDelayMin, Behaviour.ChargeFirstDelayMax));
		}
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
		QuietTime = 0.f;
	}
}

void UEnemyBrainComponent::HandleAnyDamage(const UHealthComponent& Health, float, float, AActor* const Instigator,
	bool, bool)
{
	if (bStopped || State != EBrainState::Engage || Instigator != GetOwner())
	{
		return;
	}

	const AStartechEnemyShip* const Ship = GetShip();
	if (Ship && Ship->GetBehaviour().bQuietCountsDealtDamage && Health.GetOwner() == GetLivingPlayer())
	{
		QuietTime = 0.f;
	}
}
