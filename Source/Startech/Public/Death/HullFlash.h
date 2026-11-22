#pragma once

#include "CoreMinimal.h"
#include "HullFlash.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

/**
 * Tints a ship's hull through dynamic materials on its mesh, then puts the mesh's own materials back: the death flash
 * (towards white) and the ranged enemy's charge glow (towards red). Start makes one dynamic material per slot and
 * remembers the colour and emission parameters it found; Apply blends them; Restore puts back the overrides the mesh had
 * before. Parameters a material doesn't have are skipped. Hold it in a UPROPERTY so its materials stay referenced.
 */
USTRUCT()
struct STARTECH_API FHullFlash
{
	GENERATED_BODY()

	/** Takes over Mesh's materials (restoring any earlier Start first). Does nothing for a mesh with no slots. */
	void Start(UMeshComponent& Mesh, const TArray<FName>& ColorParameters, FName EmissionParameter);

	/**
	 * Blends every colour parameter from its starting value towards Tint by Amount (0 to 1), and sets the emission
	 * parameter to its starting value (at least 1) × EmissionScale.
	 */
	void Apply(const FLinearColor& Tint, float Amount, float EmissionScale) const;

	/** Puts back the mesh's override materials from before Start. Safe when not started. */
	void Restore();

	bool IsActive() const { return bChanged; }

private:
	/** One flashing slot and its starting values. */
	struct FSlot
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		TArray<TPair<FName, FLinearColor>> Colors;
		float Emission = 0.f;
		bool bHasEmission = false;
	};

	TArray<FSlot> Slots;
	FName Emission;
	TWeakObjectPtr<UMeshComponent> FlashMesh;
	bool bChanged = false;

	/** The mesh's override materials before Start. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SavedOverrides;

	/** The dynamic materials, kept alive while the flash runs. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
};
