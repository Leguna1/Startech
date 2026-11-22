#include "WeaponMountComponent.h"

#include "Combat/CombatBalance.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TeamInterface.h"
#include "WeaponSystemComponent.h"

void UWeaponMountComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* const Owner = GetOwner())
	{
		WeaponSystem = Owner->FindComponentByClass<UWeaponSystemComponent>();
	}
}

AController* UWeaponMountComponent::GetInstigatorController() const
{
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetController() : nullptr;
}

bool UWeaponMountComponent::IsFriendly(const AActor* const HitActor) const
{
	const ITeamInterface* const OwnerTeam = Cast<const ITeamInterface>(GetOwner());
	const ITeamInterface* const HitTeam = Cast<const ITeamInterface>(HitActor);
	if (!OwnerTeam || !HitTeam)
	{
		return false;
	}

	const ETeam Team = OwnerTeam->GetTeam();
	return Team != ETeam::Neutral && Team == HitTeam->GetTeam();
}

bool UWeaponMountComponent::TraceShot(const FVector& AimPoint, const float Range, FHitResult& OutHit, FVector& OutEnd) const
{
	const FVector Start = GetComponentLocation();

	// If the aim point is on the muzzle there is no direction to it; shoot straight out of the mount.
	FVector Direction = (AimPoint - Start).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetForwardVector();
	}

	const FVector End = Start + Direction * Range;

	// Shots are a small sphere, not a zero-width line: a line aimed at a ship's centre can pass between the convex
	// pieces of its collision (SM_StarSparrow32 seen nose-on) and never hit.
	const UCombatBalance* const Balance = UCombatBalance::Get();
	const float Radius = Balance ? Balance->ShotRadius : 0.f;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponMountTrace), false, GetOwner());
	const bool bHit = Radius > 0.f
		? GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params)
		: GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);

	OutEnd = bHit ? OutHit.ImpactPoint : End;
	return bHit;
}
