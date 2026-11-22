#include "Environment/SpaceBackdrop.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASpaceBackdrop::ASpaceBackdrop()
{
	PrimaryActorTick.bCanEverTick = false;

	SkySphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkySphere"));
	SkySphere->SetMobility(EComponentMobility::Movable);
	SkySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkySphere->SetCastShadow(false);
	SkySphere->SetVisibility(false);
	SetRootComponent(SkySphere);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		SkySphere->SetStaticMesh(SphereMesh.Object);
	}
}

void ASpaceBackdrop::ApplyParams(const FBackdropParams& Params)
{
	if (BackdropMID == nullptr && BackdropMaterial != nullptr)
	{
		SkySphere->SetMaterial(0, BackdropMaterial);
		BackdropMID = SkySphere->CreateDynamicMaterialInstance(0);
	}

	SkySphere->SetVisibility(BackdropMID != nullptr);
	if (BackdropMID == nullptr)
	{
		return;
	}

	SetActorScale3D(FVector(SphereScale));
	SetActorRotation(Params.Rotation);

	BackdropMID->SetVectorParameterValue(StarTintParameter, Params.StarTint);
	BackdropMID->SetScalarParameterValue(StarDensityParameter, Params.StarDensity);
	BackdropMID->SetVectorParameterValue(NebulaColorParameter, Params.NebulaColor);
	BackdropMID->SetVectorParameterValue(NebulaColorBParameter, Params.NebulaColorB);

	const FVector SunDirection = Params.SunDirection.GetSafeNormal();
	const FLinearColor SunDirectionAsColor(static_cast<float>(SunDirection.X), static_cast<float>(SunDirection.Y), static_cast<float>(SunDirection.Z), 0.0f);
	BackdropMID->SetVectorParameterValue(SunDirectionParameter, SunDirectionAsColor);
	BackdropMID->SetVectorParameterValue(SunColorParameter, Params.SunColor);
	BackdropMID->SetScalarParameterValue(SunCosRadiusParameter, FMath::Cos(FMath::DegreesToRadians(Params.SunAngularRadiusDegrees)));
	BackdropMID->SetScalarParameterValue(SunBrightnessParameter, Params.SunBrightness);
	BackdropMID->SetScalarParameterValue(NebulaIntensityParameter, Params.NebulaIntensity);
}
