#include "Steps/SectorGenStep_AsteroidField.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Data/ProcSectorSettings.h"
#include "Environment/AsteroidField.h"
#include "Factories/EnvironmentActorFactory.h"

FName USectorGenStep_AsteroidField::GetStepName() const
{
	return TEXT("Asteroids");
}

void USectorGenStep_AsteroidField::Begin(const FSectorGenContext& Context)
{
	if (!Field.IsValid() && Context.World != nullptr)
	{
		const TSubclassOf<AAsteroidField> FieldClass = FEnvironmentActorFactory::ResolveClass(UProcSectorSettings::Get().AsteroidFieldClass);
		Field = FEnvironmentActorFactory::FindOrSpawn<AAsteroidField>(*Context.World, FieldClass);
		RemovedHandle.Reset();
	}

	if (AAsteroidField* FieldActor = Field.Get())
	{
		if (!RemovedHandle.IsValid())
		{
			RemovedHandle = FieldActor->OnAsteroidRemoved.AddUObject(this, &USectorGenStep_AsteroidField::HandleAsteroidRemoved);
		}

		FieldActor->BeginFill(Context.Params.Asteroids, Context.Definition.PlayAreaExtent, DeriveSeed(Context.Seed, SeedSalt), Context.State.RemovedAsteroidIds);
	}
}

bool USectorGenStep_AsteroidField::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	if (AAsteroidField* FieldActor = Field.Get())
	{
		return FieldActor->FillSlice(DeadlineSeconds);
	}
	return true;
}

void USectorGenStep_AsteroidField::Clear(UWorld& World)
{
	if (AAsteroidField* FieldActor = Field.Get())
	{
		FieldActor->ClearField();
	}
}

void USectorGenStep_AsteroidField::HandleAsteroidRemoved(const int32 AsteroidId)
{
	if (USectorDirectorSubsystem* Director = GetTypedOuter<USectorDirectorSubsystem>())
	{
		Director->RecordAsteroidRemoved(AsteroidId);
	}
}
