#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SectorTypes.h"
#include "PlanetActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class URotatingMovementComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * The background planet: one sphere plus a slightly larger atmosphere shell.
 * It only knows how to display the parameters it is given; it never chooses them.
 * One instance lives in the level and is restyled on every jump.
 */
UCLASS()
class PROCSECTOR_API APlanetActor : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the components and assigns the engine's basic sphere mesh. */
	APlanetActor();

	/** Moves, scales and recolours the planet and its atmosphere to match the parameters. */
	void ApplyParams(const FPlanetParams& Planet, const FAtmosphereParams& Atmosphere);

	/** Keeps the planet at a fixed offset from the player's camera, so it never gets closer however far the player flies. */
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** True treats FPlanetParams::Location as an offset from the camera (a skybox-style planet). False places it in the world. */
	UPROPERTY(EditAnywhere, Category = "Placement")
	bool bFollowViewer = true;

	/** Root that the planet is positioned by. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The planet surface sphere. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlanetMesh;

	/** The atmosphere shell sphere, hidden until AtmosphereMaterial is assigned. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> AtmosphereMesh;

	/** Spins the planet mesh at the rate given by the parameters. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<URotatingMovementComponent> Spin;

	/** Optional surface material. Empty: the mesh's own material is used (BasicShapeMaterial by default). */
	UPROPERTY(EditAnywhere, Category = "Materials")
	TObjectPtr<UMaterialInterface> PlanetMaterial;

	/** Optional atmosphere material (translucent or additive fresnel). Empty: the shell stays hidden. */
	UPROPERTY(EditAnywhere, Category = "Materials")
	TObjectPtr<UMaterialInterface> AtmosphereMaterial;

	/** Radius of the sphere mesh at scale 1. The engine's basic sphere is 50. */
	UPROPERTY(EditAnywhere, Category = "Materials", meta = (ClampMin = "1.0"))
	float MeshBaseRadius = 50.0f;

	/** Vector parameter receiving FPlanetParams::SurfaceColorA. "Color" matches BasicShapeMaterial. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName SurfaceColorAParameter = TEXT("Color");

	/** Vector parameter receiving FPlanetParams::SurfaceColorB. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName SurfaceColorBParameter = TEXT("SecondaryColor");

	/** Vector parameter receiving FPlanetParams::CloudColor. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName CloudColorParameter = TEXT("CloudColor");

	/** Scalar parameter receiving FPlanetParams::CloudCoverage. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName CloudCoverageParameter = TEXT("CloudCoverage");

	/** Scalar parameter receiving FPlanetParams::NoiseScale. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName NoiseScaleParameter = TEXT("NoiseScale");

	/** Vector parameter on the atmosphere material receiving FAtmosphereParams::Color. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName AtmosphereColorParameter = TEXT("Color");

	/** Scalar parameter on the atmosphere material receiving FAtmosphereParams::Intensity. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName AtmosphereIntensityParameter = TEXT("Intensity");

private:
	/** Creates the dynamic material instances on first use. */
	void EnsureMaterialInstances();

	/** Places the planet at ViewOffset from the camera, or from the level origin when there is no camera. */
	void UpdateFollowLocation();

	/** Offset from the camera to the planet centre, taken from FPlanetParams::Location. */
	FVector ViewOffset = FVector::ZeroVector;

	/** Runtime instance of the surface material that parameters are written to. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlanetMID;

	/** Runtime instance of the atmosphere material. Null while no AtmosphereMaterial is assigned. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AtmosphereMID;
};
