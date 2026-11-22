#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/SectorTypes.h"
#include "EnvironmentPreset.generated.h"

/**
 * Hand-authored template for a kind of destination (Earth-like, Mars-like).
 * FEnvironmentParamsFactory combines a preset with a seed to produce the final parameters,
 * so one preset yields many slightly different sectors.
 */
UCLASS(BlueprintType)
class PROCSECTOR_API UEnvironmentPreset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The un-randomised parameters this preset starts from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FEnvironmentParams BaseParams;

	/** Largest hue shift, in degrees, applied to each colour by the seed. 0 keeps colours exact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HueJitterDegrees = 10.0f;

	/** Largest relative change applied to numeric values by the seed (0.15 = plus or minus 15 percent). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScalarJitterFraction = 0.15f;

	/** True lets the seed pick the sun direction and sky rotation; false keeps the authored values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variation")
	bool bRandomizeSunAndSky = true;

#if WITH_EDITOR
	/** Editor data validation: every zone and asteroid fraction lies within 0 to 1, outer fractions are not below inner ones, spawn counts and scales are consistent, no entry both respawns and is remembered, respawn offsets are ordered, no respawn or spacing value is negative, and every asteroid type has a mesh, a weight above 0 and MaxScale not below MinScale. */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
