#include "StartechShipBase.h"

#include "Components/StaticMeshComponent.h"
#include "Death/ShipDeathComponent.h"
#include "Engine/CollisionProfile.h"
#include "HealthComponent.h"
#include "LaserMountComponent.h"
#include "LaserWeaponData.h"
#include "MapMarkerComponent.h"
#include "Station.h"
#include "ShieldDrainMountComponent.h"
#include "TargetableComponent.h"
#include "WeaponEffectsComponent.h"
#include "WeaponSystemComponent.h"

AStartechShipBase::AStartechShipBase()
{
	PrimaryActorTick.bCanEverTick = true;

	ShipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	SetRootComponent(ShipMesh);
	ShipMesh->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	ShipMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Static meshes don't generate overlap events by default; loot pickups need them.
	ShipMesh->SetGenerateOverlapEvents(true);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	WeaponSystem = CreateDefaultSubobject<UWeaponSystemComponent>(TEXT("WeaponSystem"));

	LaserMountLeft = CreateDefaultSubobject<ULaserMountComponent>(TEXT("LaserMountLeft"));
	LaserMountLeft->SetupAttachment(ShipMesh);

	LaserMountRight = CreateDefaultSubobject<ULaserMountComponent>(TEXT("LaserMountRight"));
	LaserMountRight->SetupAttachment(ShipMesh);

	ShieldDrainMount = CreateDefaultSubobject<UShieldDrainMountComponent>(TEXT("ShieldDrainMount"));
	ShieldDrainMount->SetupAttachment(ShipMesh);

	WeaponEffects = CreateDefaultSubobject<UWeaponEffectsComponent>(TEXT("WeaponEffects"));

	ShipDeath = CreateDefaultSubobject<UShipDeathComponent>(TEXT("ShipDeath"));
}

void AStartechShipBase::BeginPlay()
{
	Super::BeginPlay();

	// Bound here rather than in the constructor so the binding isn't saved into Blueprint defaults.
	HealthComponent->OnDeath.AddUniqueDynamic(this, &AStartechShipBase::HandleHealthDeath);
	ShipDeath->OnExploded.AddUObject(this, &AStartechShipBase::HandleDyingFinished);
}

void AStartechShipBase::HandleHealthDeath(AActor* const Killer)
{
	// Everything that counts happens first, so nothing waits on the dying state.
	HandleDeath(Killer);
	ShipDeath->BeginDying(GetDeathEffects());
}

void AStartechShipBase::HandleDyingFinished(const FVector& Location)
{
}

void AStartechShipBase::HandleDeath(AActor* const Killer)
{
	if (UTargetableComponent* const Targetable = FindComponentByClass<UTargetableComponent>())
	{
		Targetable->SetTargetable(false);
	}

	if (UMapMarkerComponent* const Marker = FindComponentByClass<UMapMarkerComponent>())
	{
		Marker->SetMarkerEnabled(false);
	}
}

void AStartechShipBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A wreck keeps its velocity and only drifts; its component tumbles it. Once exploded it doesn't move at all.
	if (!ShipDeath->IsAlive())
	{
		if (ShipDeath->IsDying())
		{
			MoveShip(DeltaSeconds);
		}
		return;
	}

	UpdateTurn(DeltaSeconds);
	UpdateVelocity(DeltaSeconds);
	MoveShip(DeltaSeconds);
}

void AStartechShipBase::SetThrottleInput(const float Value) { ThrottleInput = FMath::Clamp(Value, -1.f, 1.f); }
void AStartechShipBase::SetTurnInput(const float Value)     { TurnInput = FMath::Clamp(Value, -1.f, 1.f); }
void AStartechShipBase::SetVerticalInput(const float Value) { VerticalInput = FMath::Clamp(Value, -1.f, 1.f); }
void AStartechShipBase::SetStrafeInput(const float Value)   { StrafeInput = FMath::Clamp(Value, -1.f, 1.f); }

void AStartechShipBase::SetFlightScales(const float NormalScale, const float BoostScale)
{
	NormalSpeedScale = FMath::Max(NormalScale, 0.01f);
	BoostSpeedScale = FMath::Max(BoostScale, 0.f);
}

void AStartechShipBase::SetFacingLocation(const FVector& Location, const float TurnRateDegPerSec)
{
	FacingLocation = Location;
	FacingTurnRateDegPerSec = FMath::Max(TurnRateDegPerSec, 0.f);
	bHasFacingLocation = true;
}

void AStartechShipBase::ClearFacingLocation()
{
	bHasFacingLocation = false;
}

void AStartechShipBase::SetDockedStation(AStation* const Station)
{
	DockedStation = Station;
	HealthComponent->SetInvulnerable(Station != nullptr);
}

void AStartechShipBase::ResetFlightState()
{
	ShipVelocity = FVector::ZeroVector;
	YawSpeed = 0.f;
	ThrottleInput = 0.f;
	TurnInput = 0.f;
	VerticalInput = 0.f;
	StrafeInput = 0.f;
	ClearFacingLocation();
}

float AStartechShipBase::GetLaserRange() const
{
	TInlineComponentArray<ULaserMountComponent*> Mounts;
	GetComponents(Mounts);

	float Range = 0.f;
	for (const ULaserMountComponent* const Mount : Mounts)
	{
		if (const ULaserWeaponData* const Weapon = Mount->GetWeapon())
		{
			Range = FMath::Max(Range, Weapon->Range);
		}
	}
	return Range;
}

float AStartechShipBase::GetBankLimit() const
{
	return MaxBankAngle;
}

FVector AStartechShipBase::GetLevelForward() const
{
	return FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
}

void AStartechShipBase::UpdateTurn(const float DeltaSeconds)
{
	float YawStep = 0.f;
	float BankDriver = 0.f;

	if (bHasFacingLocation)
	{
		// The turn intent is ignored. YawSpeed tracks the actual turn so free flight resumes smoothly.
		YawStep = GetFacingYawStep(DeltaSeconds);
		YawSpeed = DeltaSeconds > 0.f ? YawStep / DeltaSeconds : 0.f;

		// Strafing leans the ship; otherwise it leans into the turn it is actually making.
		const float TurnLean = MaxTurnRate > 0.f ? FMath::Clamp(YawSpeed / MaxTurnRate, -1.f, 1.f) : 0.f;
		BankDriver = FMath::IsNearlyZero(StrafeInput) ? TurnLean : StrafeInput;
	}
	else
	{
		YawSpeed = FMath::FInterpTo(YawSpeed, TurnInput * MaxTurnRate, DeltaSeconds, TurnResponse);
		YawStep = YawSpeed * DeltaSeconds;
		BankDriver = TurnInput;
	}

	const float BankLimit = GetBankLimit();
	BankAngle = FMath::FInterpTo(BankAngle, BankDriver * BankLimit, DeltaSeconds, BankResponse);

	// Only tilt while moving forward: full tilt at top speed, none when hovering or reversing.
	const float ForwardSpeed = FVector::DotProduct(ShipVelocity, GetLevelForward());
	const float ForwardRatio = FMath::Clamp(ForwardSpeed / GetNormalMaxForwardSpeed(), 0.f, 1.f);
	const float TargetPitch = VerticalInput * MaxPitchAngle * ForwardRatio;
	PitchAngle = FMath::FInterpTo(PitchAngle, TargetPitch, DeltaSeconds, PitchResponse);

	const float NewYaw = GetActorRotation().Yaw + YawStep;
	SetActorRotation(FRotator(PitchAngle, NewYaw, BankAngle));
}

float AStartechShipBase::GetFacingYawStep(const float DeltaSeconds) const
{
	const FVector ToPoint = FacingLocation - GetActorLocation();
	const FVector Direction = ToPoint.GetSafeNormal();

	// Almost straight up or down (within about 2.5 degrees): no meaningful heading, so hold yaw.
	if (Direction.IsNearlyZero() || FMath::Abs(Direction.Z) > 0.999f)
	{
		return 0.f;
	}

	const float CurrentYaw = GetActorRotation().Yaw;
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToPoint.Y, ToPoint.X));
	const float Remaining = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);

	const float YawRate = FMath::Clamp(Remaining * FacingResponse, -FacingTurnRateDegPerSec, FacingTurnRateDegPerSec);
	const float Step = YawRate * DeltaSeconds;

	// A long frame or a high response must not carry the nose past the target heading.
	const float MaxStep = FMath::Abs(Remaining);
	return FMath::Clamp(Step, -MaxStep, MaxStep);
}

void AStartechShipBase::UpdateVelocity(const float DeltaSeconds)
{
	// Yaw-only forward, so banking and nose tilt never change the flight path.
	const FVector Forward = GetLevelForward();
	const FVector Right = FRotator(0.f, GetActorRotation().Yaw, 0.f).Quaternion().GetRightVector();

	const float CurrentForward = FVector::DotProduct(ShipVelocity, Forward);
	const float CurrentLateral = FVector::DotProduct(ShipVelocity, Right);
	const float CurrentVertical = ShipVelocity.Z;

	// The player's Engines upgrade scales every top speed; the boost adds to forward flight only.
	const float TargetForward = ThrottleInput >= 0.f
		? ThrottleInput * MaxForwardSpeed * NormalSpeedScale * BoostSpeedScale
		: ThrottleInput * MaxReverseSpeed * NormalSpeedScale;
	const float TargetLateral = StrafeInput * MaxStrafeSpeed * NormalSpeedScale;
	const float TargetVertical = VerticalInput * MaxVerticalSpeed * NormalSpeedScale;

	// Braking stays as it is, so a boost always eases back down at the usual rate when the throttle is released.
	const float ScaledAcceleration = Acceleration * NormalSpeedScale;
	const float ForwardRate = FMath::IsNearlyZero(ThrottleInput) ? Braking : ScaledAcceleration * BoostSpeedScale;
	const float LateralRate = FMath::IsNearlyZero(StrafeInput) ? LateralBraking : ScaledAcceleration;
	const float VerticalRate = FMath::IsNearlyZero(VerticalInput) ? Braking : ScaledAcceleration;

	const float NewForward = FMath::FInterpConstantTo(CurrentForward, TargetForward, DeltaSeconds, ForwardRate);
	const float NewLateral = FMath::FInterpConstantTo(CurrentLateral, TargetLateral, DeltaSeconds, LateralRate);
	const float NewVertical = FMath::FInterpConstantTo(CurrentVertical, TargetVertical, DeltaSeconds, VerticalRate);

	ShipVelocity = Forward * NewForward + Right * NewLateral + FVector::UpVector * NewVertical;
}

void AStartechShipBase::MoveShip(const float DeltaSeconds)
{
	const FVector Delta = ShipVelocity * DeltaSeconds;
	if (Delta.IsNearlyZero())
	{
		return;
	}

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);

	if (Hit.IsValidBlockingHit())
	{
		// Slide along the surface and drop the velocity going into it.
		const FVector Remaining = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), Hit.Normal);
		ShipVelocity = FVector::VectorPlaneProject(ShipVelocity, Hit.Normal);
		AddActorWorldOffset(Remaining, true);
	}
}
