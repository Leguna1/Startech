#include "TargetableComponent.h"

#include "Engine/World.h"
#include "TargetRegistrySubsystem.h"

void UTargetableComponent::BeginPlay()
{
	Super::BeginPlay();

	bTargetableAtStart = bTargetable;

	if (UTargetRegistrySubsystem* const Registry = GetWorld()->GetSubsystem<UTargetRegistrySubsystem>())
	{
		Registry->Register(this);
	}
}

void UTargetableComponent::SetTargetable(const bool bNewTargetable)
{
	if (bTargetable == bNewTargetable)
	{
		return;
	}

	bTargetable = bNewTargetable;

	OnTargetableChanged.Broadcast(this, bTargetable);

	if (!bTargetable)
	{
		OnTargetLost.Broadcast(this);
	}
}

void UTargetableComponent::OnAcquiredFromPool_Implementation()
{
	SetTargetable(bTargetableAtStart);
}

void UTargetableComponent::OnReleasedToPool_Implementation()
{
	SetTargetable(false);
}

void UTargetableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Announces the loss unless SetTargetable(false) already did.
	SetTargetable(false);

	if (const UWorld* const World = GetWorld())
	{
		if (UTargetRegistrySubsystem* const Registry = World->GetSubsystem<UTargetRegistrySubsystem>())
		{
			Registry->Unregister(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
