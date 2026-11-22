#include "Steps/SectorGenStep_Population.h"

#include "Engine/World.h"
#include "Environment/ViewAnchor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "Population/ActorPoolSubsystem.h"
#include "Templates/TypeHash.h"

FName USectorGenStep_Population::GetStepName() const
{
	return TEXT("Population");
}

void USectorGenStep_Population::Begin(const FSectorGenContext& Context)
{
	Plan.Reset();
	PlanCursor = 0;
	LoadedClasses.Reset();
	PendingRespawns.Reset();

	Zones = Context.Params.Zones;
	PlayAreaExtent = Context.Definition.PlayAreaExtent;
	SectorSeed = Context.Seed;

	FZoneSpawnPlanner::BuildPlan(Context.Params.Zones, Context.Definition.PlayAreaExtent, DeriveSeed(Context.Seed, SeedSalt), Plan);

	for (const FPlannedSpawn& Planned : Plan)
	{
		LoadedClasses.AddUnique(Planned.ActorClass.Get());
	}
}

bool USectorGenStep_Population::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	UActorPoolSubsystem* const Pool = Context.World != nullptr ? Context.World->GetSubsystem<UActorPoolSubsystem>() : nullptr;
	if (Pool == nullptr)
	{
		return true;
	}

	while (PlanCursor < Plan.Num())
	{
		const FPlannedSpawn& Planned = Plan[PlanCursor];
		++PlanCursor;

		const bool bAlreadyConsumed = Planned.bRememberWhenConsumed && Context.State.ConsumedSpawnIds.Contains(Planned.SpawnId);
		if (!bAlreadyConsumed)
		{
			SpawnTracked(*Pool, Planned.ActorClass, Planned.Transform, Planned.SpawnId, Planned.bRememberWhenConsumed,
				Planned.ZoneIndex, Planned.Rules);
		}

		if (FPlatformTime::Seconds() >= DeadlineSeconds)
		{
			break;
		}
	}

	if (PlanCursor >= Plan.Num())
	{
		// LoadedClasses stays: respawns take the same classes from the pool for the rest of the visit.
		Plan.Reset();
		return true;
	}
	return false;
}

void USectorGenStep_Population::Clear(UWorld& World)
{
	if (UActorPoolSubsystem* const Pool = World.GetSubsystem<UActorPoolSubsystem>())
	{
		for (const FSpawnedSectorActor& Entry : Spawned)
		{
			if (AActor* const Actor = Entry.Actor.Get())
			{
				Pool->Release(Actor);
			}
		}
	}

	// Nothing from the old sector may respawn in the next one.
	Spawned.Reset();
	PendingRespawns.Reset();
	Plan.Reset();
	PlanCursor = 0;
	LoadedClasses.Reset();
}

bool USectorGenStep_Population::TryConsumeActor(AActor& Actor, int32& OutSpawnId, bool& bOutRemember)
{
	const int32 Index = Spawned.IndexOfByPredicate([&Actor](const FSpawnedSectorActor& Entry)
	{
		return Entry.Actor.Get() == &Actor;
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	const FSpawnedSectorActor Entry = Spawned[Index];
	const FVector ConsumedLocation = Actor.GetActorLocation();
	const TSubclassOf<AActor> ActorClass = Actor.GetClass();

	OutSpawnId = Entry.SpawnId;
	bOutRemember = Entry.bRememberWhenConsumed;
	Spawned.RemoveAtSwap(Index);

	UWorld* const World = Actor.GetWorld();
	if (World != nullptr)
	{
		if (UActorPoolSubsystem* const Pool = World->GetSubsystem<UActorPoolSubsystem>())
		{
			Pool->Release(&Actor);
		}
	}

	if (World != nullptr && Entry.Rules.Respawns())
	{
		FPendingRespawn& Pending = PendingRespawns.AddDefaulted_GetRef();
		Pending.SpawnId = Entry.SpawnId;
		Pending.ActorClass = ActorClass;
		Pending.ZoneIndex = Entry.ZoneIndex;
		Pending.Rules = Entry.Rules;
		Pending.ConsumedLocation = ConsumedLocation;
		Pending.DueTime = World->GetTimeSeconds() + Entry.Rules.RespawnDelay;
		Pending.AttemptCounter = 0;
	}
	return true;
}

void USectorGenStep_Population::TickLive(UWorld& World, float DeltaSeconds)
{
	if (PendingRespawns.IsEmpty())
	{
		return;
	}

	UActorPoolSubsystem* const Pool = World.GetSubsystem<UActorPoolSubsystem>();
	if (Pool == nullptr)
	{
		return;
	}

	const double Now = World.GetTimeSeconds();
	const FVector PlayerLocation = GetPlayerLocation(World);

	// Backwards, so a finished respawn can leave the queue without skipping the next one.
	for (int32 Index = PendingRespawns.Num() - 1; Index >= 0; --Index)
	{
		FPendingRespawn& Pending = PendingRespawns[Index];
		if (Pending.DueTime > Now)
		{
			continue;
		}

		if (TryRespawn(*Pool, Pending, PlayerLocation))
		{
			PendingRespawns.RemoveAtSwap(Index);
		}
		else
		{
			// Usually the player is standing on the spot; try other points a little later.
			Pending.DueTime = Now + RetryDelaySeconds;
			++Pending.AttemptCounter;
		}
	}
}

int32 USectorGenStep_Population::CountLiveActors(const UClass* const ActorClass) const
{
	if (ActorClass == nullptr)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FSpawnedSectorActor& Entry : Spawned)
	{
		const AActor* const Actor = Entry.Actor.Get();
		if (Actor != nullptr && Actor->GetClass()->IsChildOf(ActorClass))
		{
			++Count;
		}
	}
	return Count;
}

int32 USectorGenStep_Population::CountPendingRespawns() const
{
	return PendingRespawns.Num();
}

bool USectorGenStep_Population::SpawnTracked(UActorPoolSubsystem& Pool, const TSubclassOf<AActor> ActorClass, const FTransform& Transform,
	const int32 SpawnId, const bool bRememberWhenConsumed, const int32 ZoneIndex, const FZoneSpawnRules& Rules)
{
	AActor* const Actor = Pool.Acquire(ActorClass, Transform);
	if (Actor == nullptr)
	{
		return false;
	}

	FSpawnedSectorActor& Entry = Spawned.AddDefaulted_GetRef();
	Entry.Actor = Actor;
	Entry.SpawnId = SpawnId;
	Entry.bRememberWhenConsumed = bRememberWhenConsumed;
	Entry.ZoneIndex = ZoneIndex;
	Entry.Rules = Rules;
	return true;
}

bool USectorGenStep_Population::TryRespawn(UActorPoolSubsystem& Pool, const FPendingRespawn& Pending, const FVector& PlayerLocation)
{
	if (!Zones.IsValidIndex(Pending.ZoneIndex) || Pending.ActorClass == nullptr)
	{
		// Nothing valid to bring back; drop it rather than retry forever.
		return true;
	}

	// Runtime and player-dependent, so not part of the repeatable layout, but still seeded.
	const uint32 RespawnKey = HashCombine(static_cast<uint32>(Pending.SpawnId), static_cast<uint32>(Pending.AttemptCounter));
	FRandomStream Stream(DeriveSeed(DeriveSeed(SectorSeed, RespawnSeedSalt), RespawnKey));

	TArray<FVector> Occupied;
	if (Pending.Rules.MinSpacing > 0.0f)
	{
		Occupied.Reserve(Spawned.Num());
		for (const FSpawnedSectorActor& Entry : Spawned)
		{
			if (const AActor* const Actor = Entry.Actor.Get())
			{
				Occupied.Add(Actor->GetActorLocation());
			}
		}
	}

	FVector Location;
	if (!FZoneSpawnPlanner::ChooseRespawnPoint(Stream, Zones[Pending.ZoneIndex], PlayAreaExtent, Pending.Rules,
		Pending.ConsumedLocation, PlayerLocation, Occupied, Location))
	{
		return false;
	}

	// Drawn after the point, so the yaw never changes which point is chosen.
	const FRotator Rotation(0.0, Stream.FRandRange(0.0f, 360.0f), 0.0);

	// A respawning entry is never remembered (see FZoneSpawnEntry::RespawnDelay).
	if (!SpawnTracked(Pool, Pending.ActorClass, FTransform(Rotation, Location), Pending.SpawnId, false, Pending.ZoneIndex, Pending.Rules))
	{
		return false;
	}
	return true;
}

FVector USectorGenStep_Population::GetPlayerLocation(const UWorld& World)
{
	const APlayerController* const PlayerController = World.GetFirstPlayerController();
	if (const APawn* const Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr)
	{
		return Pawn->GetActorLocation();
	}
	return FViewAnchor::GetViewLocation(&World);
}
