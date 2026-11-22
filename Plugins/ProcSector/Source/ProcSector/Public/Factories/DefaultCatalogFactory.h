#pragma once

#include "CoreMinimal.h"
#include "Data/SectorTypes.h"

class UEnvironmentPreset;
class USectorCatalog;

/** Compact description of one built-in test preset, expanded by FDefaultCatalogFactory. */
struct FDefaultPresetLook
{
	/** Object name of the preset. */
	FName Name;

	/** Primary planet surface colour. */
	FLinearColor SurfaceA = FLinearColor::Blue;

	/** Secondary planet surface colour. */
	FLinearColor SurfaceB = FLinearColor::Green;

	/** Cloud coverage, 0 to 1. */
	float CloudCoverage = 0.3f;

	/** Planet radius in Unreal units. */
	float PlanetRadius = 250000.0f;

	/** Atmosphere rim colour. */
	FLinearColor Atmosphere = FLinearColor::White;

	/** Sunlight colour. */
	FLinearColor Sun = FLinearColor::White;

	/** Fog and nebula colour. */
	FLinearColor Haze = FLinearColor::Black;

	/** Post-process tint. */
	FLinearColor Tint = FLinearColor::White;

	/** Asteroid colour. */
	FLinearColor AsteroidTint = FLinearColor::Gray;

	/** Number of asteroids before seed variation. */
	int32 AsteroidCount = 1500;

	/** Multiplier on enemy counts, so some sectors are more dangerous. */
	float EnemyDensity = 1.0f;
};

/**
 * Factory for a complete catalog built in code: four presets and four sectors joined in a ring.
 * It exists so the plugin can be tested with zero assets. Real projects assign their own
 * USectorCatalog asset in the settings and this factory is never called.
 */
class PROCSECTOR_API FDefaultCatalogFactory
{
public:
	/** Creates the test catalog as a transient object owned by Outer. */
	static USectorCatalog* CreateTransientCatalog(UObject* Outer);

private:
	/** Creates one transient preset from a compact look description. */
	static UEnvironmentPreset* MakePreset(UObject* Outer, const FDefaultPresetLook& Look);

	/** Adds the Core, Inner, Middle and Outer zones using the placeholder actor classes. */
	static void AddDefaultZones(FEnvironmentParams& Params, const float EnemyDensity);

	/** Builds one spawn zone. The fractions are of the sector's play area (see FZoneSpawnTable). */
	static FZoneSpawnTable MakeZone(const FName ZoneName, const float InnerFraction, const float OuterFraction, const float HeightFraction);

	/** Builds one spawn entry with no respawn and no spacing. */
	static FZoneSpawnEntry MakeEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount, const bool bRememberWhenConsumed);

	/** Builds an enemy entry that respawns like the old enemy spawn volume: after 30 s, at least 150 m from the player. */
	static FZoneSpawnEntry MakeEnemyEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount);

	/** Builds a loot entry that respawns like the old loot scatter volume: after 60 s, 30 to 150 m away, 80 m apart. */
	static FZoneSpawnEntry MakeLootEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount);
};
