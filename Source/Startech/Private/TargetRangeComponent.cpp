#include "TargetRangeComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "StartechHUDLog.h"
#include "StartechShipBase.h"
#include "TargetableComponent.h"
#include "TargetingComponent.h"
#include "TimerManager.h"
#include "WeaponSystemComponent.h"

UTargetRangeComponent::UTargetRangeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTargetRangeComponent::BeginPlay()
{
	Super::BeginPlay();

	UTargetingComponent* const Found = GetOwner() ? GetOwner()->FindComponentByClass<UTargetingComponent>() : nullptr;
	Targeting = Found;
	if (!Found)
	{
		return;
	}

	Found->OnTargetChanged.AddUniqueDynamic(this, &UTargetRangeComponent::HandleTargetChanged);
	HandleTargetChanged(Found->GetCurrentTarget(), Found->GetLockState());
}

void UTargetRangeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTargetingComponent* const Found = Targeting.Get())
	{
		Found->OnTargetChanged.RemoveDynamic(this, &UTargetRangeComponent::HandleTargetChanged);
	}

	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UTargetRangeComponent::HandleTargetChanged(UTargetableComponent* const NewTarget, const ETargetLockState NewState)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();
	if (NewTarget && NewState != ETargetLockState::None)
	{
		if (!Timers.IsTimerActive(EvaluationTimer))
		{
			Timers.SetTimer(EvaluationTimer, this, &UTargetRangeComponent::Evaluate, 1.f / FMath::Max(EvaluationRate, 1.f),
				true);
		}

		// A new target is rated at once rather than up to one interval later.
		Evaluate();
		return;
	}

	Timers.ClearTimer(EvaluationTimer);
	SetRangeState(ETargetRangeState::OutOfRange);
	EvaluateGroups(false);
}

void UTargetRangeComponent::Evaluate()
{
	SetRangeState(ComputeRangeState());
	EvaluateGroups(GetTarget() != nullptr);
}

bool UTargetRangeComponent::IsGroupOutOfRange(const EWeaponGroup Group) const
{
	const FGroupRange* const Entry = GroupRanges.FindByPredicate([Group](const FGroupRange& Each) { return Each.Group == Group; });
	return Entry && Entry->bOutOfRange;
}

void UTargetRangeComponent::EvaluateGroups(const bool bHasTarget)
{
	const UWeaponSystemComponent* const Weapons = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponSystemComponent>() : nullptr;
	const float Distance = bHasTarget ? GetDistanceToTarget() : 0.f;

	for (const EWeaponGroup Group : FWeaponGroups::All())
	{
		// A group with no weapon can't reach anything; with no target nothing is out of range.
		const float Range = Weapons ? Weapons->GetGroupRange(Group) : 0.f;
		const bool bOutOfRange = bHasTarget && (Range <= 0.f || Distance > Range);

		FGroupRange* Entry = GroupRanges.FindByPredicate([Group](const FGroupRange& Each) { return Each.Group == Group; });
		if (!Entry)
		{
			Entry = &GroupRanges.AddDefaulted_GetRef();
			Entry->Group = Group;
		}
		if (Entry->bOutOfRange != bOutOfRange)
		{
			Entry->bOutOfRange = bOutOfRange;
			OnGroupRangeChanged.Broadcast(Group, bOutOfRange);
		}
	}
}

ETargetRangeState UTargetRangeComponent::ComputeRangeState() const
{
	const UTargetableComponent* const Target = GetTarget();
	const AActor* const Owner = GetOwner();
	if (!Target || !Owner)
	{
		return ETargetRangeState::OutOfRange;
	}

	const float Distance = GetDistanceToTarget();

	// The target's reach: its fitted laser range, unless it is on the player's side.
	const AStartechShipBase* const TargetShip = Cast<AStartechShipBase>(Target->GetOwner());
	const AStartechShipBase* const OwnerShip = Cast<AStartechShipBase>(Owner);
	const bool bFriendly = TargetShip && OwnerShip && TargetShip->GetTeam() == OwnerShip->GetTeam()
		&& TargetShip->GetTeam() != ETeam::Neutral;
	const float TargetRange = TargetShip && !bFriendly ? TargetShip->GetLaserRange() : 0.f;

	if (TargetRange > 0.f && Distance <= TargetRange)
	{
		return ETargetRangeState::InEnemyRange;
	}

	const UWeaponSystemComponent* const Weapons = Owner->FindComponentByClass<UWeaponSystemComponent>();
	const float PlayerRange = Weapons ? Weapons->GetUsableWeaponRange() : 0.f;
	return PlayerRange > 0.f && Distance <= PlayerRange ? ETargetRangeState::PlayerAdvantage : ETargetRangeState::OutOfRange;
}

void UTargetRangeComponent::SetRangeState(const ETargetRangeState NewState)
{
	if (NewState == RangeState)
	{
		return;
	}

	RangeState = NewState;
	STARTECH_HUD_LOG(TEXT("OnRangeStateChanged: %s (distance %.0f)"), *UEnum::GetDisplayValueAsText(RangeState).ToString(),
		GetDistanceToTarget());
	OnRangeStateChanged.Broadcast(RangeState);
}

float UTargetRangeComponent::GetDistanceToTarget() const
{
	const UTargetableComponent* const Target = GetTarget();
	const AActor* const Owner = GetOwner();
	return Target && Owner ? static_cast<float>(FVector::Dist(Owner->GetActorLocation(), Target->GetAimLocation())) : 0.f;
}

const UTargetableComponent* UTargetRangeComponent::GetTarget() const
{
	const UTargetingComponent* const Found = Targeting.Get();
	const UTargetableComponent* const Target = Found && Found->GetLockState() != ETargetLockState::None
		? Found->GetCurrentTarget()
		: nullptr;

	// A dead or pooled target is still a valid object; the targetable flag says whether it still counts.
	return Target && Target->IsTargetable() ? Target : nullptr;
}
