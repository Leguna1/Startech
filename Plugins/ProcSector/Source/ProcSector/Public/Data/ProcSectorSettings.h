#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ProcSectorSettings.generated.h"

class AAsteroidField;
class ACelestialSkyActor;
class AJumpGate;
class APlanetActor;
class ASectorLightingRig;
class ASpaceBackdrop;
class USectorCatalog;
class USectorGenStep;

/**
 * Project-wide configuration, shown under Project Settings > Plugins > ProcSector
 * and saved to Config/DefaultGame.ini. Every value has a working default,
 * so the plugin runs with nothing assigned.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ProcSector"))
class PROCSECTOR_API UProcSectorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Returns the settings object. Never null. */
	static const UProcSectorSettings& Get();

	/** Places the settings page under the "Plugins" section of Project Settings. */
	virtual FName GetCategoryName() const override;

	/** Catalog of sectors to use. Empty: a four-sector test catalog is generated in code. */
	UPROPERTY(Config, EditAnywhere, Category = "Catalog")
	TSoftObjectPtr<USectorCatalog> Catalog;

	/** Sector generated when the level starts. None or unknown: the catalog's first sector. */
	UPROPERTY(Config, EditAnywhere, Category = "Catalog")
	FName StartSectorId;

	/** True generates the start sector automatically on BeginPlay. Turn off to call RequestSector yourself. */
	UPROPERTY(Config, EditAnywhere, Category = "Generation")
	bool bAutoGenerateOnBeginPlay = true;

	/** Milliseconds per frame the director may spend generating. Lower is smoother but takes more frames. */
	UPROPERTY(Config, EditAnywhere, Category = "Generation", meta = (ClampMin = "0.5", ClampMax = "33.0"))
	float GenerationBudgetMs = 4.0f;

	/** Generation steps in execution order. Empty: the built-in pipeline is used. */
	UPROPERTY(Config, EditAnywhere, Category = "Generation")
	TArray<TSoftClassPtr<USectorGenStep>> StepClasses;

	/** Asteroid instances added per batch while filling the field. */
	UPROPERTY(Config, EditAnywhere, Category = "Generation", meta = (ClampMin = "1"))
	int32 AsteroidsPerBatch = 256;

	/** Distance from a gate, toward the level centre, at which an arriving ship is placed. */
	UPROPERTY(Config, EditAnywhere, Category = "Gates", meta = (ClampMin = "0.0"))
	float GateArrivalDistance = 6000.0f;

	/** Planet actor class to spawn when the level contains none. Use a Blueprint subclass to assign materials. */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<APlanetActor> PlanetClass;

	/** Lighting rig class to spawn when the level contains none. */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<ASectorLightingRig> LightingRigClass;

	/** Backdrop class to spawn when the level contains none. */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<ASpaceBackdrop> BackdropClass;

	/** Celestial sky class (distant planets and moons) to spawn when the level contains none. */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<ACelestialSkyActor> CelestialSkyClass;

	/** Asteroid field class to spawn when the level contains none. */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AAsteroidField> AsteroidFieldClass;

	/** Jump gate class spawned for every gate link. */
	UPROPERTY(Config, EditAnywhere, Category = "Classes")
	TSoftClassPtr<AJumpGate> GateClass;

	/** Relevance components evaluated per frame. 32 re-checks 500 actors about every 16 frames. */
	UPROPERTY(Config, EditAnywhere, Category = "Relevance", meta = (ClampMin = "1"))
	int32 RelevanceChecksPerFrame = 32;
};
