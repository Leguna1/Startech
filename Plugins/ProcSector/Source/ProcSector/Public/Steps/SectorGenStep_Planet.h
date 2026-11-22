#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "SectorGenStep_Planet.generated.h"

class APlanetActor;

/** Generation step that restyles the background planet and its atmosphere. */
UCLASS()
class PROCSECTOR_API USectorGenStep_Planet : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "Planet". */
	virtual FName GetStepName() const override;

	/** Finds the level's planet actor, or spawns one of the class chosen in the settings. */
	virtual void Begin(const FSectorGenContext& Context) override;

	/** Applies the planet and atmosphere parameters. Finishes in one call. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

private:
	/** The planet actor this step drives. Weak so a deleted actor is simply found or spawned again. */
	TWeakObjectPtr<APlanetActor> Planet;
};
