#include "MapMarkerComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MapMarkerSubsystem.h"
#include "TargetableComponent.h"

void UMapMarkerComponent::BeginPlay()
{
	Super::BeginPlay();

	RegisterMarker();
}

void UMapMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterMarker();

	Super::EndPlay(EndPlayReason);
}

void UMapMarkerComponent::RegisterMarker()
{
	const UWorld* const World = GetWorld();
	if (UMapMarkerSubsystem* const Markers = World ? World->GetSubsystem<UMapMarkerSubsystem>() : nullptr)
	{
		Markers->Register(this);
	}
}

void UMapMarkerComponent::UnregisterMarker()
{
	const UWorld* const World = GetWorld();
	if (UMapMarkerSubsystem* const Markers = World ? World->GetSubsystem<UMapMarkerSubsystem>() : nullptr)
	{
		Markers->Unregister(this);
	}
}

void UMapMarkerComponent::SetMarkerEnabled(const bool bNewEnabled)
{
	if (bMarkerEnabled == bNewEnabled)
	{
		return;
	}

	bMarkerEnabled = bNewEnabled;

	const UWorld* const World = GetWorld();
	if (UMapMarkerSubsystem* const Markers = World ? World->GetSubsystem<UMapMarkerSubsystem>() : nullptr)
	{
		Markers->NotifyMarkerChanged(this);
	}
}

void UMapMarkerComponent::SetExpiryTime(const double WorldTime)
{
	ExpiryTime = WorldTime >= 0.0 ? WorldTime : -1.0;
}

float UMapMarkerComponent::GetSecondsRemaining() const
{
	const UWorld* const World = GetWorld();
	if (!HasExpiry() || !World)
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Max(ExpiryTime - World->GetTimeSeconds(), 0.0));
}

ERadarContactKind UMapMarkerComponent::GetContactKind() const
{
	switch (Type)
	{
	case EMapMarkerType::Enemy:
		return ERadarContactKind::Hostile;
	case EMapMarkerType::Loot:
	case EMapMarkerType::DroppedCargo:
		return ERadarContactKind::Loot;
	default:
		return ERadarContactKind::Neutral;
	}
}

bool UMapMarkerComponent::IsOwnerTargetable() const
{
	const AActor* const Owner = GetOwner();
	const UTargetableComponent* const Targetable = Owner ? Owner->FindComponentByClass<UTargetableComponent>() : nullptr;
	return Targetable && Targetable->IsTargetable();
}
