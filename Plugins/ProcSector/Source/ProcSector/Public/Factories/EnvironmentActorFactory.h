#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

/**
 * Factory for the one-per-level environment actors (planet, lighting rig, backdrop, asteroid field).
 * It reuses an actor already placed in the level and only spawns one when none exists,
 * which is why the plugin works both in a hand-built level and in a completely empty one.
 */
class FEnvironmentActorFactory
{
public:
	/** Returns the first actor of the type found in the world, or spawns one of ActorClass at the origin. */
	template <typename TActor>
	static TActor* FindOrSpawn(UWorld& World, const TSubclassOf<TActor> ActorClass)
	{
		TActorIterator<TActor> Existing(&World);
		if (Existing)
		{
			return *Existing;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		UClass* ClassToSpawn = ActorClass != nullptr ? ActorClass.Get() : TActor::StaticClass();
		return World.SpawnActor<TActor>(ClassToSpawn, FTransform::Identity, SpawnParameters);
	}

	/** Loads a class chosen in the settings, falling back to the native class when none is set. */
	template <typename TObject>
	static TSubclassOf<TObject> ResolveClass(const TSoftClassPtr<TObject>& SoftClass)
	{
		UClass* Loaded = SoftClass.LoadSynchronous();
		return Loaded != nullptr ? Loaded : TObject::StaticClass();
	}
};
