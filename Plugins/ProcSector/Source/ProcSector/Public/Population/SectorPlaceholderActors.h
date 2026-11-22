#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SectorPlaceholderActors.generated.h"

class UMaterialInstanceDynamic;
class URelevanceComponent;
class UStaticMeshComponent;

/**
 * Coloured basic-shape stand-in used by the built-in test catalog, so the plugin can be
 * tested before any real enemy, loot or station class exists. Not meant for the real game.
 */
UCLASS(Abstract)
class PROCSECTOR_API ASectorPlaceholderActor : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the mesh and relevance components. Subclasses pick the shape, size and colour. */
	ASectorPlaceholderActor();

protected:
	/** Applies PlaceholderColor to the mesh material. */
	virtual void BeginPlay() override;

	/** Assigns one of the engine's basic shapes and a scale. Only valid inside a subclass constructor. */
	void SetShape(const TCHAR* MeshPath, const FVector& Scale);

	/** The visible shape. Blocks everything so it can be hit by traces. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Puts the placeholder to sleep when the player is far away, to exercise the relevance system. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<URelevanceComponent> Relevance;

	/** Colour written to the "Color" parameter of BasicShapeMaterial. */
	UPROPERTY(EditAnywhere, Category = "Placeholder")
	FLinearColor PlaceholderColor = FLinearColor::White;

private:
	/** Runtime material instance the colour is written to. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColorMID;
};

/** Red cone standing in for an enemy ship. Respawns on every visit. */
UCLASS()
class PROCSECTOR_API ASectorPlaceholderEnemy : public ASectorPlaceholderActor
{
	GENERATED_BODY()

public:
	/** Sets a red cone. */
	ASectorPlaceholderEnemy();
};

/** Yellow cube standing in for world loot. Stays gone once consumed. */
UCLASS()
class PROCSECTOR_API ASectorPlaceholderLoot : public ASectorPlaceholderActor
{
	GENERATED_BODY()

public:
	/** Sets a small yellow cube. */
	ASectorPlaceholderLoot();
};

/** Large grey cylinder standing in for the central station. Stays gone once consumed. */
UCLASS()
class PROCSECTOR_API ASectorPlaceholderStation : public ASectorPlaceholderActor
{
	GENERATED_BODY()

public:
	/** Sets a large grey cylinder. */
	ASectorPlaceholderStation();
};
