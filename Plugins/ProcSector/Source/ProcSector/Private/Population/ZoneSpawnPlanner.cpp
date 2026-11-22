#include "Population/ZoneSpawnPlanner.h"

#include "Data/PlayAreaMath.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "ProcSectorLog.h"
#include "Templates/TypeHash.h"

FZoneSpawnRules FZoneSpawnRules::FromEntry(const FZoneSpawnEntry& Entry)
{
	FZoneSpawnRules Rules;
	Rules.RespawnDelay = FMath::Max(Entry.RespawnDelay, 0.0f);
	Rules.RespawnOffsetMin = FMath::Max(Entry.RespawnOffsetMin, 0.0f);
	Rules.RespawnOffsetMax = FMath::Max(Entry.RespawnOffsetMax, Rules.RespawnOffsetMin);
	Rules.MinDistanceFromPlayer = FMath::Max(Entry.MinDistanceFromPlayer, 0.0f);
	Rules.MinSpacing = FMath::Max(Entry.MinSpacing, 0.0f);
	return Rules;
}

void FZoneSpawnPlanner::BuildPlan(const TArray<FZoneSpawnTable>& Zones, const FVector& PlayAreaExtent, const int32 Seed, TArray<FPlannedSpawn>& OutPlan)
{
	FRandomStream Stream(Seed);

	// Spacing redraws come from their own stream, so they never shift the layout stream's draws.
	FRandomStream SpacingStream(static_cast<int32>(HashCombineFast(static_cast<uint32>(Seed), SpacingSeedSalt)));

	// Every point placed so far, including those of other zones, for the spacing check.
	TArray<FVector> PlacedLocations;
	PlacedLocations.Reserve(OutPlan.Num());
	for (const FPlannedSpawn& Existing : OutPlan)
	{
		PlacedLocations.Add(Existing.Transform.GetLocation());
	}

	for (int32 ZoneIndex = 0; ZoneIndex < Zones.Num(); ++ZoneIndex)
	{
		const FZoneSpawnTable& Zone = Zones[ZoneIndex];

		for (const FZoneSpawnEntry& Entry : Zone.Entries)
		{
			// Roll the count before checking the class so a broken entry does not shift later ids.
			const int32 Count = Stream.RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount));
			UClass* const ActorClass = Entry.ActorClass.LoadSynchronous();
			const FZoneSpawnRules Rules = FZoneSpawnRules::FromEntry(Entry);

			for (int32 Index = 0; Index < Count; ++Index)
			{
				FVector Location = FPlayAreaMath::RandomPointInRing(Stream, Zone.InnerFraction, Zone.OuterFraction, Zone.HeightFraction, PlayAreaExtent);
				if (Rules.MinSpacing > 0.0f)
				{
					for (int32 Redraw = 0; Redraw < MaxSpacingRedraws && !IsFarEnough(Location, PlacedLocations, Rules.MinSpacing); ++Redraw)
					{
						Location = FPlayAreaMath::RandomPointInRing(SpacingStream, Zone.InnerFraction, Zone.OuterFraction, Zone.HeightFraction, PlayAreaExtent);
					}
				}

				const FRotator Rotation(0.0, Stream.FRandRange(0.0f, 360.0f), 0.0);

				FPlannedSpawn& Planned = OutPlan.AddDefaulted_GetRef();
				Planned.SpawnId = OutPlan.Num() - 1;
				Planned.ActorClass = ActorClass;
				Planned.Transform = FTransform(Rotation, Location);
				Planned.bRememberWhenConsumed = Entry.bRememberWhenConsumed && !Rules.Respawns();
				Planned.ZoneIndex = ZoneIndex;
				Planned.Rules = Rules;
				PlacedLocations.Add(Location);
			}

			if (ActorClass == nullptr && Count > 0)
			{
				UE_LOG(LogProcSector, Warning, TEXT("Zone '%s' has a spawn entry whose class could not be loaded."), *Zone.ZoneName.ToString());
			}
		}
	}

	// Entries without a class keep their ids reserved but are never spawned.
	OutPlan.RemoveAll([](const FPlannedSpawn& Planned)
	{
		return Planned.ActorClass == nullptr;
	});
}

bool FZoneSpawnPlanner::ChooseRespawnPoint(FRandomStream& Stream, const FZoneSpawnTable& Zone, const FVector& PlayAreaExtent,
	const FZoneSpawnRules& Rules, const FVector& ConsumedLocation, const FVector& PlayerLocation,
	const TArray<FVector>& OccupiedLocations, FVector& OutLocation)
{
	const double HalfHeight = FMath::Clamp(Zone.HeightFraction, 0.0f, 1.0f) * PlayAreaExtent.Z;
	const double PlayerDistanceSq = FMath::Square(static_cast<double>(Rules.MinDistanceFromPlayer));

	for (int32 Try = 0; Try < MaxRespawnTries; ++Try)
	{
		// Four draws per try, always in this order, whether or not the try is accepted.
		const double Distance = Stream.FRandRange(Rules.RespawnOffsetMin, Rules.RespawnOffsetMax);
		const double Angle = Stream.FRandRange(0.0f, UE_TWO_PI);
		const double TargetHeight = Stream.FRandRange(-1.0f, 1.0f) * HalfHeight;

		// The vertical part aims for a height inside the zone's band, limited by the distance so the offset stays exact.
		const double Vertical = FMath::Clamp(TargetHeight - ConsumedLocation.Z, -Distance, Distance);
		const double Horizontal = FMath::Sqrt(FMath::Max(FMath::Square(Distance) - FMath::Square(Vertical), 0.0));
		const FVector Candidate = ConsumedLocation + FVector(Horizontal * FMath::Cos(Angle), Horizontal * FMath::Sin(Angle), Vertical);

		const bool bAccepted = FPlayAreaMath::IsInsideRing(Candidate, Zone.InnerFraction, Zone.OuterFraction, Zone.HeightFraction, PlayAreaExtent)
			&& FPlayAreaMath::IsInsidePlayArea(Candidate, PlayAreaExtent)
			&& FVector::DistSquared(Candidate, PlayerLocation) >= PlayerDistanceSq
			&& IsFarEnough(Candidate, OccupiedLocations, Rules.MinSpacing);
		if (bAccepted)
		{
			OutLocation = Candidate;
			return true;
		}
	}
	return false;
}

bool FZoneSpawnPlanner::IsFarEnough(const FVector& Location, const TArray<FVector>& Others, const float MinSpacing)
{
	if (MinSpacing <= 0.0f)
	{
		return true;
	}

	const double MinSpacingSq = FMath::Square(static_cast<double>(MinSpacing));
	return !Others.ContainsByPredicate([&Location, MinSpacingSq](const FVector& Other)
	{
		return FVector::DistSquared(Location, Other) < MinSpacingSq;
	});
}
