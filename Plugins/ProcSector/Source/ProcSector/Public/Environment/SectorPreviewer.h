#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SectorPreviewer.generated.h"

class UEnvironmentPreset;

/**
 * Editor helper. Place one in a level that also contains the environment actors,
 * pick a preset and seed, and press "Apply Preview" in the Details panel to see
 * the destination without pressing Play. Has no effect at runtime.
 */
UCLASS()
class PROCSECTOR_API ASectorPreviewer : public AActor
{
	GENERATED_BODY()

public:
	/** Disables ticking; this actor only exists for its Details panel button. */
	ASectorPreviewer();

	/** Builds parameters from PreviewPreset and PreviewSeed and applies them to the environment actors in this level. */
	UFUNCTION(CallInEditor, Category = "Preview")
	void ApplyPreview();

	/** Removes previewed asteroids so they are not saved into the level. */
	UFUNCTION(CallInEditor, Category = "Preview")
	void ClearPreviewAsteroids();

protected:
	/** Preset to preview. */
	UPROPERTY(EditAnywhere, Category = "Preview")
	TObjectPtr<UEnvironmentPreset> PreviewPreset;

	/** Seed to preview. Change it to see the preset's variation range. */
	UPROPERTY(EditAnywhere, Category = "Preview")
	int32 PreviewSeed = 1;

	/** Half-size of the play area the previewed asteroids are scaled to, standing in for FSectorDefinition::PlayAreaExtent (the previewer has no sector). */
	UPROPERTY(EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0"))
	FVector PreviewPlayAreaExtent = FVector(200000.0, 200000.0, 50000.0);

	/** True also fills the asteroid field, which can take a moment for large counts. */
	UPROPERTY(EditAnywhere, Category = "Preview")
	bool bPreviewAsteroids = true;
};
