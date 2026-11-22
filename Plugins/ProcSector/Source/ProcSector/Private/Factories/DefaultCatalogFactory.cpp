#include "Factories/DefaultCatalogFactory.h"

#include "Data/EnvironmentPreset.h"
#include "Data/SectorCatalog.h"
#include "Population/SectorPlaceholderActors.h"

USectorCatalog* FDefaultCatalogFactory::CreateTransientCatalog(UObject* Outer)
{
	USectorCatalog* Catalog = NewObject<USectorCatalog>(Outer, TEXT("DefaultSectorCatalog"), RF_Transient);

	TArray<FDefaultPresetLook> Looks;

	FDefaultPresetLook& Terra = Looks.AddDefaulted_GetRef();
	Terra.Name = TEXT("Terra");
	Terra.SurfaceA = FLinearColor(0.03f, 0.18f, 0.55f);
	Terra.SurfaceB = FLinearColor(0.08f, 0.35f, 0.1f);
	Terra.CloudCoverage = 0.45f;
	Terra.Atmosphere = FLinearColor(0.3f, 0.55f, 1.0f);
	Terra.Sun = FLinearColor(1.0f, 0.96f, 0.9f);
	Terra.Haze = FLinearColor(0.02f, 0.04f, 0.1f);
	Terra.AsteroidTint = FLinearColor(0.25f, 0.22f, 0.2f);

	FDefaultPresetLook& Rust = Looks.AddDefaulted_GetRef();
	Rust.Name = TEXT("Rust");
	Rust.SurfaceA = FLinearColor(0.55f, 0.2f, 0.1f);
	Rust.SurfaceB = FLinearColor(0.3f, 0.12f, 0.07f);
	Rust.CloudCoverage = 0.05f;
	Rust.PlanetRadius = 180000.0f;
	Rust.Atmosphere = FLinearColor(0.9f, 0.5f, 0.3f);
	Rust.Sun = FLinearColor(1.0f, 0.85f, 0.7f);
	Rust.Haze = FLinearColor(0.08f, 0.03f, 0.02f);
	Rust.Tint = FLinearColor(1.0f, 0.92f, 0.85f);
	Rust.AsteroidTint = FLinearColor(0.35f, 0.18f, 0.12f);
	Rust.AsteroidCount = 2200;
	Rust.EnemyDensity = 1.3f;

	FDefaultPresetLook& Glacier = Looks.AddDefaulted_GetRef();
	Glacier.Name = TEXT("Glacier");
	Glacier.SurfaceA = FLinearColor(0.7f, 0.85f, 0.95f);
	Glacier.SurfaceB = FLinearColor(0.4f, 0.6f, 0.8f);
	Glacier.CloudCoverage = 0.25f;
	Glacier.Atmosphere = FLinearColor(0.6f, 0.9f, 1.0f);
	Glacier.Sun = FLinearColor(0.85f, 0.93f, 1.0f);
	Glacier.Haze = FLinearColor(0.02f, 0.06f, 0.08f);
	Glacier.Tint = FLinearColor(0.9f, 0.97f, 1.0f);
	Glacier.AsteroidTint = FLinearColor(0.5f, 0.6f, 0.7f);
	Glacier.AsteroidCount = 900;
	Glacier.EnemyDensity = 0.8f;

	FDefaultPresetLook& Jove = Looks.AddDefaulted_GetRef();
	Jove.Name = TEXT("Jove");
	Jove.SurfaceA = FLinearColor(0.8f, 0.6f, 0.35f);
	Jove.SurfaceB = FLinearColor(0.5f, 0.3f, 0.15f);
	Jove.CloudCoverage = 0.9f;
	Jove.PlanetRadius = 400000.0f;
	Jove.Atmosphere = FLinearColor(1.0f, 0.8f, 0.5f);
	Jove.Sun = FLinearColor(1.0f, 0.9f, 0.75f);
	Jove.Haze = FLinearColor(0.06f, 0.02f, 0.08f);
	Jove.Tint = FLinearColor(1.0f, 0.95f, 0.9f);
	Jove.AsteroidTint = FLinearColor(0.3f, 0.25f, 0.3f);
	Jove.AsteroidCount = 3000;
	Jove.EnemyDensity = 1.6f;

	// One star system shared by all four sectors. Orbit radii and sizes loosely follow Earth, Mars, an ice world and Jupiter.
	const float OrbitRadii[] = { 1.0f, 1.5f, 3.0f, 5.2f };
	const float OrbitAngles[] = { 0.0f, 55.0f, 150.0f, 250.0f };
	const float OrbitHeights[] = { 0.0f, 0.08f, -0.2f, 0.3f };
	const float BodyRadii[] = { 1.0f, 0.6f, 0.9f, 6.0f };

	// Gates sit 0.925 of the way to the +X and -X edges (185000 cm in the default play area).
	const double GateFraction = 0.925;
	const int32 SectorCount = Looks.Num();

	for (int32 Index = 0; Index < SectorCount; ++Index)
	{
		const FDefaultPresetLook& Look = Looks[Index];

		FCelestialBodyDefinition& Body = Catalog->StarSystem.Bodies.AddDefaulted_GetRef();
		Body.BodyId = Look.Name;
		Body.OrbitRadius = OrbitRadii[Index];
		Body.OrbitAngleDegrees = OrbitAngles[Index];
		Body.OrbitHeight = OrbitHeights[Index];
		Body.BodyRadius = BodyRadii[Index];
		Body.ColorA = Look.SurfaceA;
		Body.ColorB = Look.SurfaceB;

		// Terra gets one moon; Jove gets two, pushed further out so its large disc does not hide them.
		if (Index == 0)
		{
			FMoonDefinition& Moon = Body.Moons.AddDefaulted_GetRef();
			Moon.MoonId = TEXT("TerraMoon");
		}
		else if (Index == 3)
		{
			FMoonDefinition& First = Body.Moons.AddDefaulted_GetRef();
			First.MoonId = TEXT("JoveMoonA");
			First.OffsetFromPlanet = FRotator(12.0, 70.0, 0.0);
			First.RelativeRadius = 0.08f;
			First.ColorA = FLinearColor(0.7f, 0.6f, 0.4f);

			FMoonDefinition& Second = Body.Moons.AddDefaulted_GetRef();
			Second.MoonId = TEXT("JoveMoonB");
			Second.OffsetFromPlanet = FRotator(-18.0, -75.0, 0.0);
			Second.RelativeRadius = 0.05f;
			Second.ColorA = FLinearColor(0.6f, 0.7f, 0.8f);
		}

		FSectorDefinition& Sector = Catalog->Sectors.AddDefaulted_GetRef();
		Sector.SectorId = Look.Name;
		Sector.BodyId = Look.Name;
		Sector.DisplayName = FText::FromName(Look.Name);
		Sector.Seed = 1000 * (Index + 1) + 1;
		Sector.Preset = MakePreset(Catalog, Look);
		Sector.PlayAreaExtent = FVector(200000.0, 200000.0, 50000.0);

		// Ring layout: the +X gate leads to the next sector, the -X gate back to the previous one.
		FGateLink& Next = Sector.Gates.AddDefaulted_GetRef();
		Next.DestinationSectorId = Looks[(Index + 1) % SectorCount].Name;
		Next.NormalizedLocation = FVector(GateFraction, 0.0, 0.0);

		FGateLink& Previous = Sector.Gates.AddDefaulted_GetRef();
		Previous.DestinationSectorId = Looks[(Index + SectorCount - 1) % SectorCount].Name;
		Previous.NormalizedLocation = FVector(-GateFraction, 0.0, 0.0);
	}

	return Catalog;
}

UEnvironmentPreset* FDefaultCatalogFactory::MakePreset(UObject* Outer, const FDefaultPresetLook& Look)
{
	UEnvironmentPreset* Preset = NewObject<UEnvironmentPreset>(Outer, Look.Name, RF_Transient);
	FEnvironmentParams& Params = Preset->BaseParams;

	Params.Planet.SurfaceColorA = Look.SurfaceA;
	Params.Planet.SurfaceColorB = Look.SurfaceB;
	Params.Planet.CloudCoverage = Look.CloudCoverage;
	Params.Planet.Radius = Look.PlanetRadius;

	Params.Atmosphere.Color = Look.Atmosphere;
	Params.Sun.Color = Look.Sun;

	Params.Ambient.SkyLightColor = Look.Atmosphere;
	Params.Ambient.FogColor = Look.Haze;
	Params.Ambient.SceneTint = Look.Tint;

	Params.Backdrop.NebulaColor = Look.Haze * 4.0f;
	Params.Backdrop.NebulaColorB = Look.Atmosphere * 0.35f;
	Params.Backdrop.NebulaIntensity = 0.25f;
	Params.Backdrop.StarTint = Look.Sun;

	Params.Asteroids.Distribution = EAsteroidDistribution::Box;
	Params.Asteroids.Count = Look.AsteroidCount;
	Params.Asteroids.InnerFraction = 0.15f;
	Params.Asteroids.OuterFraction = 0.95f;
	Params.Asteroids.HeightFraction = 0.9f;
	Params.Asteroids.Tint = Look.AsteroidTint;

	AddDefaultZones(Params, Look.EnemyDensity);
	return Preset;
}

void FDefaultCatalogFactory::AddDefaultZones(FEnvironmentParams& Params, const float EnemyDensity)
{
	UClass* EnemyClass = ASectorPlaceholderEnemy::StaticClass();
	UClass* LootClass = ASectorPlaceholderLoot::StaticClass();
	UClass* StationClass = ASectorPlaceholderStation::StaticClass();

	const auto Scaled = [EnemyDensity](const int32 Count)
	{
		return FMath::Max(1, FMath::RoundToInt(static_cast<float>(Count) * EnemyDensity));
	};

	FZoneSpawnTable Core = MakeZone(TEXT("Core"), 0.0f, 0.0f, 0.0f);
	Core.Entries.Add(MakeEntry(StationClass, 1, 1, true));
	Params.Zones.Add(Core);

	FZoneSpawnTable Inner = MakeZone(TEXT("Inner"), 0.10f, 0.35f, 0.8f);
	Inner.Entries.Add(MakeEnemyEntry(EnemyClass, Scaled(4), Scaled(6)));
	Inner.Entries.Add(MakeLootEntry(LootClass, 3, 5));
	Params.Zones.Add(Inner);

	FZoneSpawnTable Middle = MakeZone(TEXT("Middle"), 0.35f, 0.65f, 0.8f);
	Middle.Entries.Add(MakeEnemyEntry(EnemyClass, Scaled(6), Scaled(10)));
	Middle.Entries.Add(MakeLootEntry(LootClass, 4, 7));
	Params.Zones.Add(Middle);

	FZoneSpawnTable Outer = MakeZone(TEXT("Outer"), 0.65f, 0.95f, 0.8f);
	Outer.Entries.Add(MakeEnemyEntry(EnemyClass, Scaled(8), Scaled(14)));
	Outer.Entries.Add(MakeLootEntry(LootClass, 5, 9));
	Params.Zones.Add(Outer);
}

FZoneSpawnTable FDefaultCatalogFactory::MakeZone(const FName ZoneName, const float InnerFraction, const float OuterFraction, const float HeightFraction)
{
	FZoneSpawnTable Zone;
	Zone.ZoneName = ZoneName;
	Zone.InnerFraction = InnerFraction;
	Zone.OuterFraction = OuterFraction;
	Zone.HeightFraction = HeightFraction;
	return Zone;
}

FZoneSpawnEntry FDefaultCatalogFactory::MakeEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount, const bool bRememberWhenConsumed)
{
	FZoneSpawnEntry Entry;
	Entry.ActorClass = TSoftClassPtr<AActor>(ActorClass);
	Entry.MinCount = MinCount;
	Entry.MaxCount = MaxCount;
	Entry.bRememberWhenConsumed = bRememberWhenConsumed;
	return Entry;
}

FZoneSpawnEntry FDefaultCatalogFactory::MakeEnemyEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount)
{
	FZoneSpawnEntry Entry = MakeEntry(ActorClass, MinCount, MaxCount, false);
	Entry.RespawnDelay = 30.0f;
	Entry.MinDistanceFromPlayer = 15000.0f;
	return Entry;
}

FZoneSpawnEntry FDefaultCatalogFactory::MakeLootEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount)
{
	FZoneSpawnEntry Entry = MakeEntry(ActorClass, MinCount, MaxCount, false);
	Entry.RespawnDelay = 60.0f;
	Entry.RespawnOffsetMin = 3000.0f;
	Entry.RespawnOffsetMax = 15000.0f;
	Entry.MinSpacing = 8000.0f;
	return Entry;
}
