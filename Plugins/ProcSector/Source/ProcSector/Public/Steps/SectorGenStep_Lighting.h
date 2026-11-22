#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "SectorGenStep_Lighting.generated.h"

class ASectorLightingRig;

/** Generation step that applies sun, sky light, fog and scene tint. */
UCLASS()
class PROCSECTOR_API USectorGenStep_Lighting : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "Lighting". */
	virtual FName GetStepName() const override;

	/** Finds the level's lighting rig, or spawns one of the class chosen in the settings. */
	virtual void Begin(const FSectorGenContext& Context) override;

	/** Applies the sun and ambient parameters. Finishes in one call. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

private:
	/** The lighting rig this step drives. */
	TWeakObjectPtr<ASectorLightingRig> Rig;
};
