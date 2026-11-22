#include "Factories/SectorGenStepFactory.h"

#include "Core/SectorGenStep.h"
#include "ProcSectorLog.h"
#include "Steps/SectorGenStep_AsteroidField.h"
#include "Steps/SectorGenStep_Backdrop.h"
#include "Steps/SectorGenStep_CelestialSky.h"
#include "Steps/SectorGenStep_Gates.h"
#include "Steps/SectorGenStep_Lighting.h"
#include "Steps/SectorGenStep_Planet.h"
#include "Steps/SectorGenStep_Population.h"

TArray<USectorGenStep*> FSectorGenStepFactory::CreatePipeline(UObject* Outer, const TArray<TSoftClassPtr<USectorGenStep>>& StepClasses)
{
	TArray<TSubclassOf<USectorGenStep>> Classes;
	for (const TSoftClassPtr<USectorGenStep>& SoftClass : StepClasses)
	{
		if (UClass* Loaded = SoftClass.LoadSynchronous())
		{
			Classes.Add(Loaded);
		}
		else
		{
			UE_LOG(LogProcSector, Warning, TEXT("Step class '%s' could not be loaded and was skipped."), *SoftClass.ToString());
		}
	}

	if (Classes.IsEmpty())
	{
		GetDefaultStepClasses(Classes);
	}

	TArray<USectorGenStep*> Steps;
	Steps.Reserve(Classes.Num());
	for (const TSubclassOf<USectorGenStep>& StepClass : Classes)
	{
		if (StepClass != nullptr && !StepClass->HasAnyClassFlags(CLASS_Abstract))
		{
			Steps.Add(NewObject<USectorGenStep>(Outer, StepClass));
		}
	}
	return Steps;
}

void FSectorGenStepFactory::GetDefaultStepClasses(TArray<TSubclassOf<USectorGenStep>>& OutClasses)
{
	// Cheap visual steps first, so the look is ready even if a later step takes several frames.
	OutClasses.Add(USectorGenStep_Lighting::StaticClass());
	OutClasses.Add(USectorGenStep_Backdrop::StaticClass());
	OutClasses.Add(USectorGenStep_Planet::StaticClass());
	OutClasses.Add(USectorGenStep_CelestialSky::StaticClass());
	OutClasses.Add(USectorGenStep_AsteroidField::StaticClass());
	OutClasses.Add(USectorGenStep_Gates::StaticClass());
	OutClasses.Add(USectorGenStep_Population::StaticClass());
}
