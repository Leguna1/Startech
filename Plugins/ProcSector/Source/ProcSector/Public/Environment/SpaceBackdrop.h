#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SectorTypes.h"
#include "SpaceBackdrop.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * A huge inward-facing sphere showing stars and nebulae through one parameter-driven material.
 * Hidden until BackdropMaterial is assigned (in a Blueprint subclass), because the look
 * lives entirely in that material.
 */
UCLASS()
class PROCSECTOR_API ASpaceBackdrop : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the sky sphere component and assigns the engine's basic sphere mesh. */
	ASpaceBackdrop();

	/** Sends colours and densities to the material and rotates the sphere. */
	void ApplyParams(const FBackdropParams& Params);

protected:
	/** The sky sphere mesh. No collision, no shadows. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SkySphere;

	/** Sky material: unlit, two-sided, with "Is Sky" enabled. Empty: the backdrop stays hidden. */
	UPROPERTY(EditAnywhere, Category = "Materials")
	TObjectPtr<UMaterialInterface> BackdropMaterial;

	/** Uniform scale of the sphere. Must enclose the planet; 40000 gives a 20 km radius. */
	UPROPERTY(EditAnywhere, Category = "Materials", meta = (ClampMin = "1.0"))
	float SphereScale = 40000.0f;

	/** Vector parameter receiving FBackdropParams::StarTint. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName StarTintParameter = TEXT("StarTint");

	/** Scalar parameter receiving FBackdropParams::StarDensity. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName StarDensityParameter = TEXT("StarDensity");

	/** Vector parameter receiving FBackdropParams::NebulaColor. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName NebulaColorParameter = TEXT("NebulaColor");

	/** Vector parameter receiving FBackdropParams::NebulaColorB. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName NebulaColorBParameter = TEXT("NebulaColorB");

	/** Scalar parameter receiving FBackdropParams::NebulaIntensity. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName NebulaIntensityParameter = TEXT("NebulaIntensity");

	/** Vector parameter receiving the world-space direction toward the sun (FBackdropParams::SunDirection) in its RGB. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName SunDirectionParameter = TEXT("SunDirection");

	/** Vector parameter receiving FBackdropParams::SunColor. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName SunColorParameter = TEXT("SunColor");

	/** Scalar parameter receiving the cosine of FBackdropParams::SunAngularRadiusDegrees, which the material compares against. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName SunCosRadiusParameter = TEXT("SunCosRadius");

	/** Scalar parameter receiving FBackdropParams::SunBrightness. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName SunBrightnessParameter = TEXT("SunBrightness");

private:
	/** Runtime instance of BackdropMaterial. Null while no material is assigned. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackdropMID;
};
