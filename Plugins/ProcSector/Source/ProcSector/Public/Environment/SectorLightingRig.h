#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SectorTypes.h"
#include "SectorLightingRig.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class USceneComponent;
class USkyLightComponent;

/**
 * Owns every light-related component of the level: sun, planetshine, sky light, fog and a scene tint.
 * It only applies the parameters it is given. If you place your own lights in the level,
 * remove them or they will stack with this rig.
 */
UCLASS()
class PROCSECTOR_API ASectorLightingRig : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the movable light, sky light, fog and unbound post-process components. */
	ASectorLightingRig();

	/** Applies sun and planetshine direction, colours, intensities, fog and tint, then refreshes the sky light. */
	void ApplyParams(const FSunParams& SunParams, const FAmbientParams& Ambient);

protected:
	/** Root component; the rig's position does not matter. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The sun. Movable so colour and direction can change at runtime. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> Sun;

	/** Light reflected off the near planet. No shadows; fills the side of objects facing away from the sun. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> Planetshine;

	/** Fill light. Uses real-time capture so it follows the backdrop and fog automatically. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkyLightComponent> SkyLight;

	/** Thin fog that gives each sector a coloured haze. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	/** Unbound post-process volume used only for the scene colour tint. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> PostProcess;
};
