#include "Population/ActorPoolSubsystem.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Population/ComponentTickUtils.h"
#include "Population/Poolable.h"
#include "ProcSectorLog.h"

AActor* UActorPoolSubsystem::Acquire(TSubclassOf<AActor> ActorClass, const FTransform& Transform)
{
	if (ActorClass == nullptr)
	{
		return nullptr;
	}

	AActor* Actor = nullptr;
	{
		FActorPoolBucket& Bucket = FindOrAddBucket(ActorClass.Get());
		while (Actor == nullptr && !Bucket.Free.IsEmpty())
		{
			// Stored actors can have been destroyed from outside; skip those.
			AActor* Candidate = Bucket.Free.Pop();
			if (IsValid(Candidate))
			{
				Actor = Candidate;
			}
		}
	}

	if (Actor == nullptr)
	{
		Actor = SpawnNew(ActorClass.Get(), Transform);
	}

	if (Actor == nullptr)
	{
		UE_LOG(LogProcSector, Warning, TEXT("Pool could not spawn an actor of class '%s'."), *ActorClass->GetName());
		return nullptr;
	}

	// Look the bucket up again: spawning can run code that adds buckets and moves the array.
	++FindOrAddBucket(ActorClass.Get()).ActiveCount;
	Activate(*Actor, Transform);
	return Actor;
}

void UActorPoolSubsystem::Release(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	FActorPoolBucket& Bucket = FindOrAddBucket(Actor->GetClass());
	if (Bucket.Free.Contains(Actor))
	{
		return;
	}

	Deactivate(*Actor);
	Bucket.Free.Add(Actor);
	Bucket.ActiveCount = FMath::Max(0, Bucket.ActiveCount - 1);
}

void UActorPoolSubsystem::Prewarm(TSubclassOf<AActor> ActorClass, int32 Count)
{
	if (ActorClass == nullptr)
	{
		return;
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (AActor* Actor = SpawnNew(ActorClass.Get(), FTransform::Identity))
		{
			Deactivate(*Actor);
			// Look the bucket up each time: spawning can run code that adds buckets and moves the array.
			FindOrAddBucket(ActorClass.Get()).Free.Add(Actor);
		}
	}
}

int32 UActorPoolSubsystem::GetActiveCount() const
{
	int32 Total = 0;
	for (const FActorPoolBucket& Bucket : Buckets)
	{
		Total += Bucket.ActiveCount;
	}
	return Total;
}

int32 UActorPoolSubsystem::GetFreeCount() const
{
	int32 Total = 0;
	for (const FActorPoolBucket& Bucket : Buckets)
	{
		Total += Bucket.Free.Num();
	}
	return Total;
}

FActorPoolBucket& UActorPoolSubsystem::FindOrAddBucket(UClass* ActorClass)
{
	FActorPoolBucket* Existing = Buckets.FindByPredicate([ActorClass](const FActorPoolBucket& Bucket)
	{
		return Bucket.ActorClass == ActorClass;
	});

	if (Existing != nullptr)
	{
		return *Existing;
	}

	FActorPoolBucket& Added = Buckets.AddDefaulted_GetRef();
	Added.ActorClass = ActorClass;
	return Added;
}

AActor* UActorPoolSubsystem::SpawnNew(UClass* ActorClass, const FTransform& Transform) const
{
	UWorld* World = GetWorld();
	if (World == nullptr || ActorClass == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, Transform, SpawnParameters);
}

void UActorPoolSubsystem::Activate(AActor& Actor, const FTransform& Transform)
{
	Actor.SetActorTransform(Transform, false, nullptr, ETeleportType::ResetPhysics);
	Actor.SetActorHiddenInGame(false);
	Actor.SetActorEnableCollision(true);

	// Restore ticking only for classes that tick by default.
	const AActor* ClassDefault = Actor.GetClass()->GetDefaultObject<AActor>();
	Actor.SetActorTickEnabled(ClassDefault->PrimaryActorTick.bStartWithTickEnabled);
	FComponentTickUtils::SetComponentTicks(Actor, true);

	NotifyPoolables(Actor, true);
}

void UActorPoolSubsystem::Deactivate(AActor& Actor)
{
	NotifyPoolables(Actor, false);

	Actor.SetActorTickEnabled(false);
	FComponentTickUtils::SetComponentTicks(Actor, false);
	FComponentTickUtils::ClearAllTimers(Actor);
	Actor.SetActorEnableCollision(false);
	Actor.SetActorHiddenInGame(true);
}

void UActorPoolSubsystem::NotifyPoolables(AActor& Actor, const bool bAcquired)
{
	const auto Notify = [bAcquired](UObject* Object)
	{
		if (Object != nullptr && Object->GetClass()->ImplementsInterface(UPoolable::StaticClass()))
		{
			if (bAcquired)
			{
				IPoolable::Execute_OnAcquiredFromPool(Object);
			}
			else
			{
				IPoolable::Execute_OnReleasedToPool(Object);
			}
		}
	};

	Notify(&Actor);

	TInlineComponentArray<UActorComponent*> Components;
	Actor.GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		Notify(Component);
	}
}
