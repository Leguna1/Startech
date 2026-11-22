#include "Population/RelevanceComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Population/ComponentTickUtils.h"
#include "Population/RelevanceSubsystem.h"

URelevanceComponent::URelevanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URelevanceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		bOwnerTicksWhenAwake = Owner->PrimaryActorTick.bStartWithTickEnabled;
	}

	if (UWorld* World = GetWorld())
	{
		if (URelevanceSubsystem* Subsystem = World->GetSubsystem<URelevanceSubsystem>())
		{
			Subsystem->Register(*this);
		}
	}
}

void URelevanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URelevanceSubsystem* Subsystem = World->GetSubsystem<URelevanceSubsystem>())
		{
			Subsystem->Unregister(*this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void URelevanceComponent::Evaluate(const FVector& ViewerLocation)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	const double DistanceSquared = FVector::DistSquared(Owner->GetActorLocation(), ViewerLocation);

	if (bAwake && DistanceSquared > FMath::Square(static_cast<double>(SleepDistance)))
	{
		SetAwake(false);
	}
	else if (!bAwake && DistanceSquared < FMath::Square(static_cast<double>(WakeDistance)))
	{
		SetAwake(true);
	}
}

void URelevanceComponent::SetAwake(const bool bNewAwake)
{
	if (bAwake == bNewAwake)
	{
		return;
	}
	bAwake = bNewAwake;

	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorTickEnabled(bAwake && bOwnerTicksWhenAwake);
		FComponentTickUtils::SetComponentTicks(*Owner, bAwake);

		if (bDisableCollisionWhenAsleep)
		{
			Owner->SetActorEnableCollision(bAwake);
		}
		if (bHideWhenAsleep)
		{
			Owner->SetActorHiddenInGame(!bAwake);
		}
	}

	OnRelevanceChanged.Broadcast(bAwake);
}

bool URelevanceComponent::IsAwake() const
{
	return bAwake;
}

bool URelevanceComponent::IsEvaluationEnabled() const
{
	return bEvaluationEnabled;
}

void URelevanceComponent::OnAcquiredFromPool_Implementation()
{
	// The pool has just switched everything on, so the stored state must match without re-applying it.
	bEvaluationEnabled = true;
	if (!bAwake)
	{
		bAwake = true;
		OnRelevanceChanged.Broadcast(true);
	}
}

void URelevanceComponent::OnReleasedToPool_Implementation()
{
	bEvaluationEnabled = false;
}
