#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

/**
 * Switches an actor's component ticks and timers together. Turning off only the actor's tick leaves its
 * components (AI, weapons, regeneration) running, so the pool and the relevance component use these helpers
 * to stop and restart a whole actor. Header-only; shared by UActorPoolSubsystem and URelevanceComponent.
 */
struct FComponentTickUtils
{
	/**
	 * Enables or disables the tick of every component of the actor that can tick. Enabling restores each
	 * component to its own PrimaryComponentTick.bStartWithTickEnabled, so components that start switched off stay off.
	 */
	static void SetComponentTicks(AActor& Actor, const bool bEnable)
	{
		TInlineComponentArray<UActorComponent*> Components(&Actor);
		for (UActorComponent* const Component : Components)
		{
			if (Component != nullptr && Component->PrimaryComponentTick.bCanEverTick)
			{
				Component->SetComponentTickEnabled(bEnable && Component->PrimaryComponentTick.bStartWithTickEnabled);
			}
		}
	}

	/** Clears every timer bound to the actor and to each of its components. */
	static void ClearAllTimers(AActor& Actor)
	{
		UWorld* const World = Actor.GetWorld();
		if (World == nullptr)
		{
			return;
		}

		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearAllTimersForObject(&Actor);

		TInlineComponentArray<UActorComponent*> Components(&Actor);
		for (UActorComponent* const Component : Components)
		{
			if (Component != nullptr)
			{
				Timers.ClearAllTimersForObject(Component);
			}
		}
	}
};
