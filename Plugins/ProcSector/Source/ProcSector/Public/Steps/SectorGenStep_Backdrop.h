#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "SectorGenStep_Backdrop.generated.h"

class ASpaceBackdrop;

/** Generation step that restyles the star and nebula sky sphere. */
UCLASS()
class PROCSECTOR_API USectorGenStep_Backdrop : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "Backdrop". */
	virtual FName GetStepName() const override;

	/** Finds the level's backdrop actor, or spawns one of the class chosen in the settings. */
	virtual void Begin(const FSectorGenContext& Context) override;

	/** Applies the backdrop parameters. Finishes in one call. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

private:
	/** The backdrop actor this step drives. */
	TWeakObjectPtr<ASpaceBackdrop> Backdrop;
};
