#include "TargetingComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "StartechHUDLog.h"
#include "TargetableComponent.h"
#include "TargetRegistrySubsystem.h"

UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTargetingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (LockState == ETargetLockState::Hard)
	{
		if (IsHardLockValid())
		{
			return;
		}

		// Broken lock: release it, then fall through and look for a new soft lock this frame.
		SetTarget(nullptr, ETargetLockState::None);
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		SetTarget(nullptr, ETargetLockState::None);
		return;
	}

	UTargetableComponent* const SoftTarget = FindSoftTarget(ViewLocation, ViewRotation.Vector());
	SetTarget(SoftTarget, SoftTarget ? ETargetLockState::Soft : ETargetLockState::None);
}

void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Tell the last target it is no longer locked.
	SetTarget(nullptr, ETargetLockState::None);

	Super::EndPlay(EndPlayReason);
}

void UTargetingComponent::ToggleHardLock()
{
	if (LockState == ETargetLockState::Hard)
	{
		SetTarget(nullptr, ETargetLockState::None);
	}
	else if (UTargetableComponent* const Target = CurrentTarget.Get())
	{
		SetTarget(Target, ETargetLockState::Hard);
	}
}

AActor* UTargetingComponent::GetCurrentTargetActor() const
{
	const UTargetableComponent* const Target = CurrentTarget.Get();
	return Target ? Target->GetOwner() : nullptr;
}

FVector UTargetingComponent::GetAimPoint() const
{
	if (const UTargetableComponent* const Target = CurrentTarget.Get();
		Target && LockState != ETargetLockState::None)
	{
		return Target->GetAimLocation();
	}

	// No target: straight ahead of the ship.
	const AActor* const Owner = GetOwner();
	return Owner ? Owner->GetActorLocation() + Owner->GetActorForwardVector() * LockRange : FVector::ZeroVector;
}

bool UTargetingComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	const APlayerController* const PC = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	if (!PC)
	{
		return false;
	}

	PC->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

void UTargetingComponent::HandleTargetLost(UTargetableComponent* const LostTarget)
{
	// Same release path for soft and hard locks, so all the usual notifications fire.
	if (LostTarget && LostTarget == CurrentTarget.Get())
	{
		SetTarget(nullptr, ETargetLockState::None);
	}
}

bool UTargetingComponent::IsHardLockValid() const
{
	const UTargetableComponent* const Target = CurrentTarget.Get();
	const AActor* const Owner = GetOwner();
	if (!IsValid(Target) || !Target->IsTargetable() || !Owner)
	{
		return false;
	}

	const float BreakRange = LockRange * HardLockBreakRangeScale;
	return FVector::DistSquared(Owner->GetActorLocation(), Target->GetAimLocation()) <= FMath::Square(BreakRange);
}

UTargetableComponent* UTargetingComponent::FindSoftTarget(const FVector& ViewLocation, const FVector& ViewDirection) const
{
	const AActor* const Owner = GetOwner();
	const UTargetRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UTargetRegistrySubsystem>();
	if (!Owner || !Registry)
	{
		return nullptr;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const float RangeSquared = FMath::Square(LockRange);

	// Smallest angle is largest cosine, so compare dot products and skip the acos.
	float BestCos = FMath::Cos(FMath::DegreesToRadians(SoftLockHalfAngle));
	UTargetableComponent* Best = nullptr;

	for (const TWeakObjectPtr<UTargetableComponent>& Entry : Registry->GetTargets())
	{
		UTargetableComponent* const Candidate = Entry.Get();

		// Never lock on to our own ship.
		if (!IsValid(Candidate) || !Candidate->IsTargetable() || Candidate->GetOwner() == Owner)
		{
			continue;
		}

		const FVector AimLocation = Candidate->GetAimLocation();
		if (FVector::DistSquared(OwnerLocation, AimLocation) > RangeSquared)
		{
			continue;
		}

		const FVector ToTarget = (AimLocation - ViewLocation).GetSafeNormal();
		const float Cos = FVector::DotProduct(ViewDirection, ToTarget);
		if (Cos > BestCos)
		{
			BestCos = Cos;
			Best = Candidate;
		}
	}

	return Best;
}

void UTargetingComponent::LockTarget(UTargetableComponent* const Target)
{
	if (!Target)
	{
		ClearTarget();
		return;
	}
	SetTarget(Target, ETargetLockState::Hard);
}

void UTargetingComponent::ClearTarget()
{
	SetTarget(nullptr, ETargetLockState::None);
}

void UTargetingComponent::SetTarget(UTargetableComponent* const NewTarget, const ETargetLockState NewState)
{
	UTargetableComponent* const OldTarget = CurrentTarget.Get();
	if (OldTarget == NewTarget && LockState == NewState)
	{
		return;
	}

	const bool bWasHard = LockState == ETargetLockState::Hard;
	const bool bIsHard = NewState == ETargetLockState::Hard;

	CurrentTarget = NewTarget;
	LockState = NewState;

	if (OldTarget != NewTarget)
	{
		// Removing a binding during the old target's own OnTargetLost broadcast is safe.
		if (OldTarget)
		{
			OldTarget->OnTargetLost.RemoveDynamic(this, &UTargetingComponent::HandleTargetLost);
		}
		if (NewTarget)
		{
			NewTarget->OnTargetLost.AddUniqueDynamic(this, &UTargetingComponent::HandleTargetLost);
		}
	}

	if (OldTarget && OldTarget != NewTarget)
	{
		OldTarget->OnLockStateChanged.Broadcast(ETargetLockState::None);
	}

	if (NewTarget)
	{
		NewTarget->OnLockStateChanged.Broadcast(NewState);
	}

	STARTECH_HUD_LOG(TEXT("OnTargetChanged: %s, %s"), *GetNameSafe(NewTarget ? NewTarget->GetOwner() : nullptr),
		*UEnum::GetDisplayValueAsText(NewState).ToString());
	OnTargetChanged.Broadcast(NewTarget, NewState);

	if (!bWasHard && bIsHard)
	{
		STARTECH_HUD_LOG(TEXT("OnHardLockEngaged"));
		OnHardLockEngaged.Broadcast();
	}
	else if (bWasHard && !bIsHard)
	{
		STARTECH_HUD_LOG(TEXT("OnHardLockReleased"));
		OnHardLockReleased.Broadcast();
	}
}
