#include "Environment/SectorPreviewer.h"

#include "Data/EnvironmentPreset.h"
#include "EngineUtils.h"
#include "Environment/AsteroidField.h"
#include "Environment/PlanetActor.h"
#include "Environment/SectorLightingRig.h"
#include "Environment/SpaceBackdrop.h"
#include "Factories/EnvironmentParamsFactory.h"
#include "ProcSectorLog.h"

ASectorPreviewer::ASectorPreviewer()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASectorPreviewer::ApplyPreview()
{
	UWorld* World = GetWorld();
	if (World == nullptr || PreviewPreset == nullptr)
	{
		UE_LOG(LogProcSector, Warning, TEXT("SectorPreviewer: assign a PreviewPreset first."));
		return;
	}

	FEnvironmentParams Params = FEnvironmentParamsFactory::Build(*PreviewPreset, PreviewSeed);
	FEnvironmentParamsFactory::ResolvePlanetshine(Params);

	for (TActorIterator<APlanetActor> It(World); It; ++It)
	{
		It->ApplyParams(Params.Planet, Params.Atmosphere);
	}
	for (TActorIterator<ASectorLightingRig> It(World); It; ++It)
	{
		It->ApplyParams(Params.Sun, Params.Ambient);
	}
	for (TActorIterator<ASpaceBackdrop> It(World); It; ++It)
	{
		It->ApplyParams(Params.Backdrop);
	}

	if (bPreviewAsteroids)
	{
		for (TActorIterator<AAsteroidField> It(World); It; ++It)
		{
			It->BeginFill(Params.Asteroids, PreviewPlayAreaExtent, PreviewSeed, TArray<int32>());
			while (!It->FillSlice(TNumericLimits<double>::Max()))
			{
			}
		}
	}
}

void ASectorPreviewer::ClearPreviewAsteroids()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AAsteroidField> It(World); It; ++It)
		{
			It->ClearField();
		}
	}
}
