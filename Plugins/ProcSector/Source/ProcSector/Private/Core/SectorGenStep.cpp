#include "Core/SectorGenStep.h"

#include "Templates/TypeHash.h"

FName USectorGenStep::GetStepName() const
{
	return GetClass()->GetFName();
}

void USectorGenStep::Begin(const FSectorGenContext& Context)
{
}

bool USectorGenStep::TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)
{
	return true;
}

void USectorGenStep::Clear(UWorld& World)
{
}

bool USectorGenStep::TryConsumeActor(AActor& Actor, int32& OutSpawnId, bool& bOutRemember)
{
	return false;
}

void USectorGenStep::TickLive(UWorld& World, float DeltaSeconds)
{
}

int32 USectorGenStep::CountLiveActors(const UClass* ActorClass) const
{
	return 0;
}

int32 USectorGenStep::CountPendingRespawns() const
{
	return 0;
}

int32 USectorGenStep::DeriveSeed(const int32 SectorSeed, const uint32 StepSalt)
{
	return static_cast<int32>(HashCombineFast(static_cast<uint32>(SectorSeed), StepSalt));
}
