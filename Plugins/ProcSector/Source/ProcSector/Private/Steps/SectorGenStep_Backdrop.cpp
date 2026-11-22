#include "Steps/SectorGenStep_Backdrop.h"

#include "Data/ProcSectorSettings.h"
#include "Environment/SpaceBackdrop.h"
#include "Factories/EnvironmentActorFactory.h"

FName USectorGenStep_Backdrop::GetStepName() const
{
	return TEXT("Backdrop");
}

void USectorGenStep_Backdrop::Begin(const FSectorGenContext& Context)
{
	if (!Backdrop.IsValid() && Context.World != nullptr)
	{
		const TSubclassOf<ASpaceBackdrop> BackdropClass = FEnvironmentActorFactory::ResolveClass(UProcSectorSettings::Get().BackdropClass);
		Backdrop = FEnvironmentActorFactory::FindOrSpawn<ASpaceBackdrop>(*Context.World, BackdropClass);
	}
}

bool USectorGenStep_Backdrop::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	if (ASpaceBackdrop* BackdropActor = Backdrop.Get())
	{
		BackdropActor->ApplyParams(Context.Params.Backdrop);
	}
	return true;
}
