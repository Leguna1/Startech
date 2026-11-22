#include "Steps/SectorGenStep_Gates.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Data/PlayAreaMath.h"
#include "Data/ProcSectorSettings.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "Factories/EnvironmentActorFactory.h"
#include "Gates/JumpGate.h"
#include "Population/ActorPoolSubsystem.h"

FName USectorGenStep_Gates::GetStepName() const
{
	return TEXT("Gates");
}

bool USectorGenStep_Gates::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	UActorPoolSubsystem* Pool = Context.World != nullptr ? Context.World->GetSubsystem<UActorPoolSubsystem>() : nullptr;
	if (Pool == nullptr)
	{
		return true;
	}

	const USectorDirectorSubsystem* Director = GetTypedOuter<USectorDirectorSubsystem>();
	const USectorCatalog* Catalog = Director != nullptr ? Director->GetCatalog() : nullptr;
	const TSubclassOf<AJumpGate> GateClass = FEnvironmentActorFactory::ResolveClass(UProcSectorSettings::Get().GateClass);

	for (const FGateLink& Link : Context.Definition.Gates)
	{
		const FVector WorldLocation = FPlayAreaMath::ToWorld(Link.NormalizedLocation, Context.Definition.PlayAreaExtent);
		AJumpGate* Gate = Cast<AJumpGate>(Pool->Acquire(GateClass, FTransform(WorldLocation)));
		if (Gate == nullptr)
		{
			continue;
		}

		const FSectorDefinition* Destination = Catalog != nullptr ? Catalog->FindSector(Link.DestinationSectorId) : nullptr;
		const FText Label = Destination != nullptr ? Destination->DisplayName : FText::FromName(Link.DestinationSectorId);

		Gate->Configure(Link, WorldLocation, Label);
		ActiveGates.Add(Gate);
	}

	return true;
}

void USectorGenStep_Gates::Clear(UWorld& World)
{
	if (UActorPoolSubsystem* Pool = World.GetSubsystem<UActorPoolSubsystem>())
	{
		for (const TWeakObjectPtr<AJumpGate>& Gate : ActiveGates)
		{
			if (AJumpGate* GateActor = Gate.Get())
			{
				Pool->Release(GateActor);
			}
		}
	}
	ActiveGates.Reset();
}
