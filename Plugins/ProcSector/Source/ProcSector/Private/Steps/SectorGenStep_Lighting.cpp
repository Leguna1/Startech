#include "Steps/SectorGenStep_Lighting.h"

#include "Data/ProcSectorSettings.h"
#include "Environment/SectorLightingRig.h"
#include "Factories/EnvironmentActorFactory.h"

FName USectorGenStep_Lighting::GetStepName() const
{
	return TEXT("Lighting");
}

void USectorGenStep_Lighting::Begin(const FSectorGenContext& Context)
{
	if (!Rig.IsValid() && Context.World != nullptr)
	{
		const TSubclassOf<ASectorLightingRig> RigClass = FEnvironmentActorFactory::ResolveClass(UProcSectorSettings::Get().LightingRigClass);
		Rig = FEnvironmentActorFactory::FindOrSpawn<ASectorLightingRig>(*Context.World, RigClass);
	}
}

bool USectorGenStep_Lighting::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	if (ASectorLightingRig* RigActor = Rig.Get())
	{
		RigActor->ApplyParams(Context.Params.Sun, Context.Params.Ambient);
	}
	return true;
}
