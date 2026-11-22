#include "Steps/SectorGenStep_Planet.h"

#include "Data/ProcSectorSettings.h"
#include "Environment/PlanetActor.h"
#include "Factories/EnvironmentActorFactory.h"

FName USectorGenStep_Planet::GetStepName() const
{
	return TEXT("Planet");
}

void USectorGenStep_Planet::Begin(const FSectorGenContext& Context)
{
	if (!Planet.IsValid() && Context.World != nullptr)
	{
		const TSubclassOf<APlanetActor> PlanetClass = FEnvironmentActorFactory::ResolveClass(UProcSectorSettings::Get().PlanetClass);
		Planet = FEnvironmentActorFactory::FindOrSpawn<APlanetActor>(*Context.World, PlanetClass);
	}
}

bool USectorGenStep_Planet::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	if (APlanetActor* PlanetActor = Planet.Get())
	{
		PlanetActor->ApplyParams(Context.Params.Planet, Context.Params.Atmosphere);
	}
	return true;
}
