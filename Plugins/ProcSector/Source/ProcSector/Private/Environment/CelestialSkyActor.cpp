#include "Environment/CelestialSkyActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Environment/ViewAnchor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACelestialSkyActor::ACelestialSkyActor()
{
	// Tick after the camera has moved this frame, so the bodies never lag a frame behind it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh = SphereMesh.Object;
	}
}

void ACelestialSkyActor::ApplyParams(const TArray<FSkyBodyParams>& Bodies)
{
	EnsureBodyCount(Bodies.Num());

	for (int32 Index = 0; Index < BodyComponents.Num(); ++Index)
	{
		UStaticMeshComponent* Component = BodyComponents[Index];
		if (Component == nullptr)
		{
			continue;
		}

		const bool bUsed = Bodies.IsValidIndex(Index);
		Component->SetVisibility(bUsed);
		if (!bUsed)
		{
			continue;
		}

		const FSkyBodyParams& Body = Bodies[Index];

		// Convert the angle the body should cover into a sphere size at the fixed placement distance.
		const float AngularRadians = FMath::DegreesToRadians(FMath::Clamp(Body.AngularRadiusDegrees, 0.01f, 45.0f));
		const float WorldRadius = BodyDistance * FMath::Tan(AngularRadians);

		Component->SetRelativeLocation(Body.Direction.GetSafeNormal() * BodyDistance);
		Component->SetRelativeScale3D(FVector(WorldRadius / FMath::Max(MeshBaseRadius, 1.0f)));

		if (UMaterialInstanceDynamic* MID = BodyMIDs.IsValidIndex(Index) ? BodyMIDs[Index].Get() : nullptr)
		{
			MID->SetVectorParameterValue(ColorAParameter, Body.ColorA);
			MID->SetVectorParameterValue(ColorBParameter, Body.ColorB);
		}
	}

	SetActorLocation(FViewAnchor::GetViewLocation(GetWorld()));
}

void ACelestialSkyActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SetActorLocation(FViewAnchor::GetViewLocation(GetWorld()));
}

void ACelestialSkyActor::EnsureBodyCount(const int32 Count)
{
	while (BodyComponents.Num() < Count)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(BodyMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		if (BodyMaterial != nullptr)
		{
			Component->SetMaterial(0, BodyMaterial);
		}
		Component->SetupAttachment(SceneRoot);
		Component->RegisterComponent();

		BodyComponents.Add(Component);
		BodyMIDs.Add(Component->CreateDynamicMaterialInstance(0));
	}
}
