#include "SectorBounds.h"

#include "Components/BoxComponent.h"

namespace
{
	// How far inside the box GetClosestPointInside places its result, in unscaled local cm.
	constexpr float InsideMargin = 500.f;
}

ASectorBounds::ASectorBounds()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	Area->InitBoxExtent(FVector(200000.f, 200000.f, 50000.f));
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetGenerateOverlapEvents(false);
	Area->SetCanEverAffectNavigation(false);
	SetRootComponent(Area);
}

bool ASectorBounds::IsInside(const FVector& Location) const
{
	// Local to the box, so a rotated or scaled sector works.
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FVector ASectorBounds::GetClosestPointInside(const FVector& Location) const
{
	const FTransform& BoxTransform = Area->GetComponentTransform();
	const FVector Local = BoxTransform.InverseTransformPosition(Location);

	// A box thinner than twice the margin collapses to its centre on that axis rather than inverting.
	const FVector Limit = (Area->GetUnscaledBoxExtent() - FVector(InsideMargin)).ComponentMax(FVector::ZeroVector);
	const FVector Clamped(
		FMath::Clamp(Local.X, -Limit.X, Limit.X),
		FMath::Clamp(Local.Y, -Limit.Y, Limit.Y),
		FMath::Clamp(Local.Z, -Limit.Z, Limit.Z));

	return BoxTransform.TransformPosition(Clamped);
}

FVector ASectorBounds::GetCenter() const
{
	return Area->GetComponentLocation();
}
