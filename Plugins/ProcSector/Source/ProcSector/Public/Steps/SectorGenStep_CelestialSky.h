#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "SectorGenStep_CelestialSky.generated.h"

class ACelestialSkyActor;

/** Generation step that shows the distant planets and moons visible from the sector. */
UCLASS()
class PROCSECTOR_API USectorGenStep_CelestialSky : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "CelestialSky". */
	virtual FName GetStepName() const override;

	/** Finds the level's celestial sky actor, or spawns one of the class chosen in the settings. */
	virtual void Begin(const FSectorGenContext& Context) override;

	/** Applies the sector's sky bodies. Finishes in one call. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

private:
	/** The celestial sky actor this step drives. */
	TWeakObjectPtr<ACelestialSkyActor> Sky;
};
