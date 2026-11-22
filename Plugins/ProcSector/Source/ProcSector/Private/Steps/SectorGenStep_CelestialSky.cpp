#include "Steps/SectorGenStep_CelestialSky.h"

#include "Data/ProcSectorSettings.h"
#include "Environment/CelestialSkyActor.h"
#include "Factories/EnvironmentActorFactory.h"

FName USectorGenStep_CelestialSky::GetStepName() const
{
	return TEXT("CelestialSky");
}

void USectorGenStep_CelestialSky::Begin(const FSectorGenContext& Context)
{
	if (!Sky.IsValid() && Context.World != nullptr)
	{
		const TSubclassOf<ACelestialSkyActor> SkyClass = FEnvironmentActorFactory::ResolveClass(UProcSectorSettings::Get().CelestialSkyClass);
		Sky = FEnvironmentActorFactory::FindOrSpawn<ACelestialSkyActor>(*Context.World, SkyClass);
	}
}

bool USectorGenStep_CelestialSky::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	if (ACelestialSkyActor* SkyActor = Sky.Get())
	{
		SkyActor->ApplyParams(Context.Params.SkyBodies);
	}
	return true;
}
