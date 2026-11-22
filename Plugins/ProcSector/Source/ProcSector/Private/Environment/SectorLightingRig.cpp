#include "Environment/SectorLightingRig.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"

ASectorLightingRig::ASectorLightingRig()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(SceneRoot);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLight(true);
	Sun->ForwardShadingPriority = 1; 

	Planetshine = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Planetshine"));
	Planetshine->SetupAttachment(SceneRoot);
	Planetshine->SetMobility(EComponentMobility::Movable);
	Planetshine->SetCastShadows(false);
	Planetshine->SetAtmosphereSunLight(false);
	Planetshine->SetVisibility(false);
	Planetshine->SetVisibility(false);
	Planetshine->ForwardShadingPriority = 0; 

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(SceneRoot);
	Fog->SetVisibility(false);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	PostProcess->bUnbound = true;
}

void ASectorLightingRig::ApplyParams(const FSunParams& SunParams, const FAmbientParams& Ambient)
{
	Sun->SetWorldRotation(SunParams.Rotation);
	Sun->SetLightColor(SunParams.Color);
	Sun->SetIntensity(SunParams.Intensity);

	const bool bUsePlanetshine = Ambient.PlanetshineIntensity > 0.0f;
	Planetshine->SetVisibility(bUsePlanetshine);
	if (bUsePlanetshine)
	{
		Planetshine->SetWorldRotation(Ambient.PlanetshineRotation);
		Planetshine->SetLightColor(Ambient.PlanetshineColor);
		Planetshine->SetIntensity(Ambient.PlanetshineIntensity);
	}

	SkyLight->SetLightColor(Ambient.SkyLightColor);
	SkyLight->SetIntensity(Ambient.SkyLightIntensity);

	const bool bUseFog = Ambient.FogDensity > 0.0f;
	Fog->SetVisibility(bUseFog);
	if (bUseFog)
	{
		Fog->SetFogInscatteringColor(Ambient.FogColor);
		Fog->SetFogDensity(Ambient.FogDensity);
	}

	PostProcess->Settings.bOverride_SceneColorTint = true;
	PostProcess->Settings.SceneColorTint = Ambient.SceneTint;

	// Harmless with real-time capture on; required if a subclass switches it off.
	SkyLight->RecaptureSky();
}
