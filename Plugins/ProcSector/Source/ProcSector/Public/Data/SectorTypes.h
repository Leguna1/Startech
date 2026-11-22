#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "SectorTypes.generated.h"

class AActor;
class UEnvironmentPreset;
class UMaterialInterface;
class UStaticMesh;

/** Phase of the sector director's regeneration cycle. */
UENUM(BlueprintType)
enum class ESectorPhase : uint8
{
	/** Nothing is being generated; the current sector is playable. */
	Idle,
	/** The previous sector's generated content is about to be cleared. */
	TearingDown,
	/** Generation steps are running, a few milliseconds per frame. */
	Generating
};

/** Shape of the volume asteroids are scattered in. */
UENUM(BlueprintType)
enum class EAsteroidDistribution : uint8
{
	/** A flat ring around the level origin between InnerFraction and OuterFraction, limited vertically by HeightFraction. Elliptical when the play area is not square. */
	Belt,
	/** A shell between InnerFraction and OuterFraction in every direction, stretched per axis by the play area into an ellipsoid. HeightFraction is not used. */
	Sphere,
	/** The whole play-area box filled evenly, keeping out of a central ellipse of InnerFraction so the station area stays clear. OuterFraction scales X and Y, HeightFraction scales Z. */
	Box
};

/** Look and placement of the background planet. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FPlanetParams
{
	GENERATED_BODY()

	/** False hides the planet entirely (deep-space sectors). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	bool bVisible = true;

	/** World location of the planet centre. Keep it well outside the playable bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	FVector Location = FVector(0.0, 600000.0, -150000.0);

	/** Planet radius in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet", meta = (ClampMin = "1.0"))
	float Radius = 250000.0f;

	/** Primary surface colour (oceans on an Earth-like, dust on a Mars-like). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	FLinearColor SurfaceColorA = FLinearColor(0.05f, 0.2f, 0.6f);

	/** Secondary surface colour (land masses, rock, ice). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	FLinearColor SurfaceColorB = FLinearColor(0.1f, 0.4f, 0.12f);

	/** Colour of the cloud layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	FLinearColor CloudColor = FLinearColor::White;

	/** How much of the surface clouds cover, 0 (none) to 1 (overcast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CloudCoverage = 0.4f;

	/** Scale of the surface noise in the planet material; larger means smaller features. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet", meta = (ClampMin = "0.01"))
	float NoiseScale = 3.0f;

	/** Planet spin around its vertical axis in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet")
	float SpinDegreesPerSecond = 0.5f;
};

/** Look of the thin glowing shell around the planet. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FAtmosphereParams
{
	GENERATED_BODY()

	/** Colour of the atmospheric rim glow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere")
	FLinearColor Color = FLinearColor(0.3f, 0.55f, 1.0f);

	/** Brightness multiplier of the rim glow. 0 disables the shell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere", meta = (ClampMin = "0.0"))
	float Intensity = 1.5f;

	/** Shell size relative to the planet (1.03 = 3 percent larger than the surface). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atmosphere", meta = (ClampMin = "1.0"))
	float ThicknessScale = 1.03f;
};

/** The sector's sun, implemented as one movable directional light. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FSunParams
{
	GENERATED_BODY()

	/** Direction the sunlight travels in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FRotator Rotation = FRotator(-25.0, 40.0, 0.0);

	/** Sunlight colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun")
	FLinearColor Color = FLinearColor(1.0f, 0.96f, 0.9f);

	/** Directional light intensity in lux. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun", meta = (ClampMin = "0.0"))
	float Intensity = 8.0f;
};

/** Fill light, fog and colour grading that tint the whole sector. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FAmbientParams
{
	GENERATED_BODY()

	/** Sky light colour (fills the shadowed side of objects). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	FLinearColor SkyLightColor = FLinearColor(0.5f, 0.6f, 1.0f);

	/** Sky light intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient", meta = (ClampMin = "0.0"))
	float SkyLightIntensity = 1.0f;

	/** Fog colour; reads as a faint nebula haze over long distances. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	FLinearColor FogColor = FLinearColor(0.02f, 0.04f, 0.1f);

	/** Exponential fog density. 0 turns fog off. At this level's scale use about 0.000002; larger values turn the lower half of the view opaque. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient", meta = (ClampMin = "0.0"))
	float FogDensity = 0.0f;

	/** Post-process tint multiplied over the final image. White means no tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	FLinearColor SceneTint = FLinearColor::White;

	/** Strength of the light reflected off the near planet, as a fraction of the sun's intensity. 0 turns planetshine off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Planetshine", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PlanetshineStrength = 0.1f;

	/**
	 * Share of planetshine kept when the player sees the planet's night side (the sun behind it).
	 * 1 ignores the planet's phase; 0 is physically correct but leaves dark sides black again.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Planetshine", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PlanetshineMinimumPhase = 0.35f;

	/** Direction the planetshine travels in, from the planet toward the player. Derived by FEnvironmentParamsFactory::ResolvePlanetshine. */
	UPROPERTY(BlueprintReadWrite, Category = "Ambient|Planetshine")
	FRotator PlanetshineRotation = FRotator::ZeroRotator;

	/** Planetshine colour, taken from the planet's surface colours. Derived. */
	UPROPERTY(BlueprintReadWrite, Category = "Ambient|Planetshine")
	FLinearColor PlanetshineColor = FLinearColor::White;

	/** Final planetshine intensity in lux, after strength and phase. 0 hides the light. Derived. */
	UPROPERTY(BlueprintReadWrite, Category = "Ambient|Planetshine")
	float PlanetshineIntensity = 0.0f;
};

/** Look of the star and nebula sky sphere. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FBackdropParams
{
	GENERATED_BODY()

	/** Colour multiplied over the stars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop")
	FLinearColor StarTint = FLinearColor::White;

	/** Star density, 0 (empty) to 1 (dense). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StarDensity = 0.5f;

	/** Nebula colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop")
	FLinearColor NebulaColor = FLinearColor(0.1f, 0.2f, 0.6f);

	/** Colour of the second nebula layer (the blue channel of the sky mask texture). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop")
	FLinearColor NebulaColorB = FLinearColor(0.4f, 0.15f, 0.6f);

	/** Nebula brightness. 0 hides the nebula. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop", meta = (ClampMin = "0.0"))
	float NebulaIntensity = 0.5f;

	/** Rotation of the whole sky sphere, so two sectors never show the same sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop")
	FRotator Rotation = FRotator::ZeroRotator;

	/** World-space direction from the viewer toward the sun. Derived from the sun light or the star system; not authored. */
	UPROPERTY(BlueprintReadWrite, Category = "Backdrop")
	FVector SunDirection = FVector::ForwardVector;

	/** Colour of the visible sun disc. Derived; not authored. */
	UPROPERTY(BlueprintReadWrite, Category = "Backdrop")
	FLinearColor SunColor = FLinearColor::White;

	/** Half the angle the sun disc covers in the sky, in degrees. The real sun seen from Earth is about 0.27. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float SunAngularRadiusDegrees = 1.5f;

	/** Emissive strength of the sun disc. Values well above 1 make it bloom. 0 hides the disc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop", meta = (ClampMin = "0.0"))
	float SunBrightness = 15.0f;
};

/** One distant body (neighbouring planet or moon) as it appears in the current sector's sky. Derived from the star system. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FSkyBodyParams
{
	GENERATED_BODY()

	/** Name of the body, for debugging. */
	UPROPERTY(BlueprintReadWrite, Category = "Sky")
	FName BodyId;

	/** World-space direction from the viewer toward the body. */
	UPROPERTY(BlueprintReadWrite, Category = "Sky")
	FVector Direction = FVector::ForwardVector;

	/** Half the angle the body covers in the sky, in degrees. */
	UPROPERTY(BlueprintReadWrite, Category = "Sky")
	float AngularRadiusDegrees = 1.0f;

	/** Primary surface colour. */
	UPROPERTY(BlueprintReadWrite, Category = "Sky")
	FLinearColor ColorA = FLinearColor::Gray;

	/** Secondary surface colour. */
	UPROPERTY(BlueprintReadWrite, Category = "Sky")
	FLinearColor ColorB = FLinearColor::Gray;
};

/**
 * One kind of asteroid in a field: its mesh, look, size and share of the field's Count.
 * Each type is drawn by its own instanced mesh component, so each type adds a draw call.
 */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FAsteroidTypeParams
{
	GENERATED_BODY()

	/** Mesh drawn for this type. Loaded when the field is filled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Optional material for this type. Empty uses the mesh's own material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids")
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Share of the field's Count, relative to the weights of the other types (3 next to 1 is three times as many). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Smallest per-axis scale applied to this type's mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.01"))
	float MinScale = 2.0f;

	/** Largest per-axis scale applied to this type's mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.01"))
	float MaxScale = 12.0f;

	/** Colour written to the material's tint parameter ("Color") for this type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids")
	FLinearColor Tint = FLinearColor(0.25f, 0.22f, 0.2f);
};

/** Asteroids scattered around the level origin. Distances are fractions of the sector's FSectorDefinition::PlayAreaExtent. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FAsteroidFieldParams
{
	GENERATED_BODY()

	/** Shape of the volume the asteroids fill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids")
	EAsteroidDistribution Distribution = EAsteroidDistribution::Sphere;

	/** Number of asteroid instances to generate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0"))
	int32 Count = 1500;

	/** Inner edge of the field as a fraction of the play area's horizontal half-size (0 = the origin, 1 = the box edge). Box: radius of the central clearing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InnerFraction = 0.15f;

	/** Outer edge of the field as a fraction of the play area's horizontal half-size. Must not be below InnerFraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OuterFraction = 0.95f;

	/** Vertical reach as a fraction of the play area's half-height; 1 reaches the floor and ceiling. Used by Belt and Box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightFraction = 0.9f;

	/** Smallest per-axis scale applied to an asteroid mesh. Only used when Types is empty; each type has its own range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.01"))
	float MinScale = 2.0f;

	/** Largest per-axis scale applied to an asteroid mesh. Only used when Types is empty; each type has its own range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids", meta = (ClampMin = "0.01"))
	float MaxScale = 12.0f;

	/** Colour sent to the asteroid material. Only used when Types is empty; each type has its own tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids")
	FLinearColor Tint = FLinearColor(0.25f, 0.22f, 0.2f);

	/**
	 * Kinds of asteroid in the field, each taking a weighted share of Count. Empty: every asteroid uses the field actor's
	 * own mesh with MinScale, MaxScale and Tint above, exactly as before types existed. Three to five types is the sweet spot;
	 * each type is one more draw call.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroids")
	TArray<FAsteroidTypeParams> Types;
};

/** One actor type to spawn inside a zone. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FZoneSpawnEntry
{
	GENERATED_BODY()

	/** Actor class to spawn (enemy, loot, station, NPC). Loaded when the sector is generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSoftClassPtr<AActor> ActorClass;

	/** Fewest instances of this class in the zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 MinCount = 1;

	/** Most instances of this class in the zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 MaxCount = 1;

	/**
	 * True: once consumed (looted, destroyed) it stays gone when the player returns to the sector.
	 * False: it respawns on every visit, which suits regular enemies.
	 * Respawn or remember, not both: an entry with RespawnDelay above 0 is never remembered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bRememberWhenConsumed = false;

	/**
	 * Seconds after a spawn is consumed (looted, killed) before it respawns during the same visit.
	 * 0 keeps it gone until the next visit. Above 0, bRememberWhenConsumed is ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "0", Units = "s"))
	float RespawnDelay = 0.0f;

	/**
	 * Smallest distance from the consumed spot to the respawn point, in centimetres.
	 * A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "0", Units = "cm"))
	float RespawnOffsetMin = 3000.0f;

	/**
	 * Largest distance from the consumed spot to the respawn point, in centimetres. Not below RespawnOffsetMin.
	 * A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "0", Units = "cm"))
	float RespawnOffsetMax = 15000.0f;

	/**
	 * A respawn never appears closer than this to the player, in centimetres; it waits until the player is far enough.
	 * A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn", meta = (ClampMin = "0", Units = "cm"))
	float MinDistanceFromPlayer = 15000.0f;

	/**
	 * Minimum distance to any other spawn of this sector, in centimetres, in the initial layout and on respawn.
	 * 0 disables it. A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0", Units = "cm"))
	float MinSpacing = 0.0f;
};

/**
 * A ring around the level origin and what spawns inside it.
 * The ring is defined in unit space and multiplied per axis by the sector's FSectorDefinition::PlayAreaExtent,
 * so a non-square play area gives an elliptical ring that still fits the box.
 */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FZoneSpawnTable
{
	GENERATED_BODY()

	/** Designer-facing zone name (Core, Inner, Middle, Outer). Used in logs only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FName ZoneName;

	/** Inner edge of the ring as a fraction of the play area's horizontal half-size. 0 with OuterFraction 0 places everything at the origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InnerFraction = 0.0f;

	/** Outer edge of the ring as a fraction of the play area's horizontal half-size; 1 touches the box edge. Must not be below InnerFraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OuterFraction = 0.0f;

	/** Vertical reach of the zone as a fraction of the play area's half-height; 1 reaches the floor and ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightFraction = 0.0f;

	/** Actor types spawned in this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	TArray<FZoneSpawnEntry> Entries;
};

/** Everything that describes how one destination looks and what it contains. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FEnvironmentParams
{
	GENERATED_BODY()

	/** Background planet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FPlanetParams Planet;

	/** Glow shell around the planet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FAtmosphereParams Atmosphere;

	/** Sun light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FSunParams Sun;

	/** Sky light, fog and scene tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FAmbientParams Ambient;

	/** Star and nebula sky sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FBackdropParams Backdrop;

	/** Asteroid belt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	FAsteroidFieldParams Asteroids;

	/** Spawn zones, usually ordered from the centre outward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
	TArray<FZoneSpawnTable> Zones;

	/** Distant planets and moons visible from this sector. Filled from the star system; not authored. */
	UPROPERTY(BlueprintReadWrite, Category = "Environment")
	TArray<FSkyBodyParams> SkyBodies;
};

/** A moon of a body. Only shown while the player is in that body's sector. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FMoonDefinition
{
	GENERATED_BODY()

	/** Name of the moon, for debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	FName MoonId;

	/** Where the moon sits in the sky relative to its planet: pitch is up or down, yaw is left or right, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	FRotator OffsetFromPlanet = FRotator(15.0, 40.0, 0.0);

	/** Apparent size as a fraction of the planet's apparent size (0.2 looks like a moon a fifth as wide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float RelativeRadius = 0.2f;

	/** Primary surface colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	FLinearColor ColorA = FLinearColor(0.5f, 0.5f, 0.5f);

	/** Secondary surface colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	FLinearColor ColorB = FLinearColor(0.3f, 0.3f, 0.3f);
};

/** A planet of the star system. Sectors refer to it by BodyId. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FCelestialBodyDefinition
{
	GENERATED_BODY()

	/** Unique name; FSectorDefinition::BodyId refers to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FName BodyId;

	/** Distance from the sun in arbitrary orbit units. Only the ratios between bodies matter (Earth 1, Mars 1.5, Jupiter 5.2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (ClampMin = "0.01"))
	float OrbitRadius = 1.0f;

	/** Position along the orbit, in degrees. Spread the bodies out so they appear in different parts of the sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float OrbitAngleDegrees = 0.0f;

	/** Height above the orbital plane in orbit units. Small values keep bodies from lining up exactly on the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float OrbitHeight = 0.0f;

	/** Relative size of the body (Earth 1, Mars 0.5, Jupiter 11). Larger bodies look bigger from other sectors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (ClampMin = "0.01"))
	float BodyRadius = 1.0f;

	/** Primary surface colour when seen from another sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FLinearColor ColorA = FLinearColor::Gray;

	/** Secondary surface colour when seen from another sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FLinearColor ColorB = FLinearColor::Gray;

	/** Moons shown while the player is in this body's sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	TArray<FMoonDefinition> Moons;
};

/**
 * One sun and its planets. Every sector is a place in this system, so the sun's direction and size
 * and the positions of the other planets are worked out from where the sector really is.
 */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FStarSystemDefinition
{
	GENERATED_BODY()

	/** Colour of the sunlight and the sun disc in every sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System")
	FLinearColor SunColor = FLinearColor(1.0f, 0.95f, 0.85f);

	/** Half-angle of the sun disc, in degrees, as seen from orbit radius 1. It shrinks in proportion further out. Exaggerated on purpose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System", meta = (ClampMin = "0.0"))
	float SunAngularRadiusAtUnitOrbit = 2.0f;

	/** How fast sunlight dims with orbit radius. 0 keeps every sector equally bright; 2 is physically correct and very dark far out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SunFalloffExponent = 0.4f;

	/** Half-angle, in degrees, of a body of radius 1 seen from one orbit unit away. Raise it to make neighbours easier to see. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System", meta = (ClampMin = "0.0"))
	float BodyAngularRadiusAtUnitDistance = 0.9f;

	/** Smallest half-angle a neighbouring body is drawn at, so distant ones stay visible as planets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System", meta = (ClampMin = "0.0"))
	float MinBodyAngularRadius = 0.35f;

	/** Largest half-angle a neighbouring body is drawn at, so none rivals the planet you are next to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System", meta = (ClampMin = "0.0"))
	float MaxBodyAngularRadius = 3.0f;

	/** The planets. Empty: the star system is not used and sectors keep their preset sun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star System")
	TArray<FCelestialBodyDefinition> Bodies;
};

/** One jump gate inside a sector. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FGateLink
{
	GENERATED_BODY()

	/** Sector the gate leads to. Must exist in the same catalog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate")
	FName DestinationSectorId;

	/**
	 * Position of the gate inside the play area, each axis from -1 to 1, multiplied by the sector's PlayAreaExtent.
	 * Keep it at least 0.05 away from the edge, or the arriving player starts in the bounds warning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gate", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	FVector NormalizedLocation = FVector::ZeroVector;
};

/** One destination the player can travel to. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FSectorDefinition
{
	GENERATED_BODY()

	/** Unique identifier used by gates, saves and console commands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector")
	FName SectorId;

	/** Player-facing name, shown on gate labels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector")
	FText DisplayName;

	/** Seed for every random choice in this sector. Same seed, same sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector")
	int32 Seed = 0;

	/** Planet of the catalog's star system this sector is next to. None: the sector keeps its preset sun and shows no neighbours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector")
	FName BodyId;

	/** Preset the sector's environment is built from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector")
	TObjectPtr<UEnvironmentPreset> Preset = nullptr;

	/**
	 * Half-size of the playable box, centred on the level origin, in centimetres.
	 * Zones, asteroids and gates scale from it, and UPlayAreaSyncComponent sizes the bounds box from it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector", meta = (ClampMin = "1.0"))
	FVector PlayAreaExtent = FVector(200000.0, 200000.0, 50000.0);

	/** Jump gates present in this sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector")
	TArray<FGateLink> Gates;
};

/** Changes the player has made to a sector, reapplied whenever it is regenerated. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FSectorState
{
	GENERATED_BODY()

	/** Sector these changes belong to. */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FName SectorId;

	/** Ids of asteroids that were mined or destroyed. */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<int32> RemovedAsteroidIds;

	/** Spawn ids of remembered actors that were consumed (looted, destroyed). */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<int32> ConsumedSpawnIds;
};

/** How long one generation step took during the last regeneration. */
USTRUCT(BlueprintType)
struct PROCSECTOR_API FSectorStepTiming
{
	GENERATED_BODY()

	/** Name of the step, from USectorGenStep::GetStepName. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	FName StepName;

	/** Total time the step spent working, summed over all frames, in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float Milliseconds = 0.0f;
};
