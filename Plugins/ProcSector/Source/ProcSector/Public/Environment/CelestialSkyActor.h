#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SectorTypes.h"
#include "CelestialSkyActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Draws the distant planets and moons of the current sector as small lit spheres.
 * The actor follows the camera, so the bodies behave like part of the sky: they keep
 * their direction and size wherever the player flies and can never be reached.
 * Each body is its own sphere so it can reuse the planet material; there are only a handful.
 */
UCLASS()
class PROCSECTOR_API ACelestialSkyActor : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the root and loads the engine's basic sphere mesh. */
	ACelestialSkyActor();

	/** Shows one sphere per sky body with the given direction, size and colours, and hides any spare spheres. */
	void ApplyParams(const TArray<FSkyBodyParams>& Bodies);

	/** Moves the actor to the camera every frame, after the camera has been updated. */
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Root that is moved to the camera; every body sphere is attached to it. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Mesh used for every body. The engine's basic sphere by default. */
	UPROPERTY(EditAnywhere, Category = "Materials")
	TObjectPtr<UStaticMesh> BodyMesh;

	/** Optional material for the bodies; use your planet material so neighbours match it. Empty: the mesh's own material. */
	UPROPERTY(EditAnywhere, Category = "Materials")
	TObjectPtr<UMaterialInterface> BodyMaterial;

	/** Radius of BodyMesh at scale 1. The engine's basic sphere is 50. */
	UPROPERTY(EditAnywhere, Category = "Materials", meta = (ClampMin = "1.0"))
	float MeshBaseRadius = 50.0f;

	/**
	 * Distance from the camera at which bodies are placed. It must be farther than the near planet
	 * (so that planet can hide them) and, added to the level radius, smaller than the backdrop sphere's radius.
	 */
	UPROPERTY(EditAnywhere, Category = "Placement", meta = (ClampMin = "1000.0"))
	float BodyDistance = 1500000.0f;

	/** Vector parameter receiving FSkyBodyParams::ColorA. "Color" matches BasicShapeMaterial and the planet material. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName ColorAParameter = TEXT("Color");

	/** Vector parameter receiving FSkyBodyParams::ColorB. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName ColorBParameter = TEXT("SecondaryColor");

private:
	/** Creates sphere components until at least Count exist. Components are kept and reused between sectors. */
	void EnsureBodyCount(const int32 Count);

	/** One sphere per body slot. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BodyComponents;

	/** Runtime material instance of each sphere, index-matched with BodyComponents. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;
};
