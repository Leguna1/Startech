#include "Environment/PlanetActor.h"

#include "Components/SceneComponent.h"
#include "Environment/ViewAnchor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APlanetActor::APlanetActor()
{
	// Tick after the camera has moved this frame, so the planet never lags a frame behind it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	PlanetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetMesh"));
	PlanetMesh->SetupAttachment(SceneRoot);
	PlanetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlanetMesh->SetCastShadow(false);

	AtmosphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AtmosphereMesh"));
	AtmosphereMesh->SetupAttachment(SceneRoot);
	AtmosphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AtmosphereMesh->SetCastShadow(false);
	AtmosphereMesh->SetVisibility(false);

	Spin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	Spin->SetUpdatedComponent(PlanetMesh);
	Spin->RotationRate = FRotator::ZeroRotator;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		PlanetMesh->SetStaticMesh(SphereMesh.Object);
		AtmosphereMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void APlanetActor::ApplyParams(const FPlanetParams& Planet, const FAtmosphereParams& Atmosphere)
{
	EnsureMaterialInstances();

	SetActorHiddenInGame(!Planet.bVisible);
	ViewOffset = Planet.Location;
	UpdateFollowLocation();

	const float PlanetScale = Planet.Radius / FMath::Max(MeshBaseRadius, 1.0f);
	PlanetMesh->SetRelativeScale3D(FVector(PlanetScale));
	AtmosphereMesh->SetRelativeScale3D(FVector(PlanetScale * Atmosphere.ThicknessScale));

	Spin->RotationRate = FRotator(0.0, Planet.SpinDegreesPerSecond, 0.0);

	if (PlanetMID != nullptr)
	{
		PlanetMID->SetVectorParameterValue(SurfaceColorAParameter, Planet.SurfaceColorA);
		PlanetMID->SetVectorParameterValue(SurfaceColorBParameter, Planet.SurfaceColorB);
		PlanetMID->SetVectorParameterValue(CloudColorParameter, Planet.CloudColor);
		PlanetMID->SetScalarParameterValue(CloudCoverageParameter, Planet.CloudCoverage);
		PlanetMID->SetScalarParameterValue(NoiseScaleParameter, Planet.NoiseScale);
	}

	const bool bShowAtmosphere = AtmosphereMID != nullptr && Atmosphere.Intensity > 0.0f;
	AtmosphereMesh->SetVisibility(bShowAtmosphere);
	if (bShowAtmosphere)
	{
		AtmosphereMID->SetVectorParameterValue(AtmosphereColorParameter, Atmosphere.Color);
		AtmosphereMID->SetScalarParameterValue(AtmosphereIntensityParameter, Atmosphere.Intensity);
	}
}

void APlanetActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFollowViewer)
	{
		UpdateFollowLocation();
	}
}

void APlanetActor::UpdateFollowLocation()
{
	const FVector Anchor = bFollowViewer ? FViewAnchor::GetViewLocation(GetWorld()) : FVector::ZeroVector;
	SetActorLocation(Anchor + ViewOffset);
}

void APlanetActor::EnsureMaterialInstances()
{
	if (PlanetMID == nullptr)
	{
		if (PlanetMaterial != nullptr)
		{
			PlanetMesh->SetMaterial(0, PlanetMaterial);
		}
		PlanetMID = PlanetMesh->CreateDynamicMaterialInstance(0);
	}

	if (AtmosphereMID == nullptr && AtmosphereMaterial != nullptr)
	{
		AtmosphereMesh->SetMaterial(0, AtmosphereMaterial);
		AtmosphereMID = AtmosphereMesh->CreateDynamicMaterialInstance(0);
	}
}
