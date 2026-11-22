#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "SectorGenStep_Gates.generated.h"

class AJumpGate;

/** Generation step that places one pooled jump gate for every gate link of the sector. */
UCLASS()
class PROCSECTOR_API USectorGenStep_Gates : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "Gates". */
	virtual FName GetStepName() const override;

	/** Takes gates from the pool and places them at each gate link's NormalizedLocation scaled by the sector's PlayAreaExtent. Finishes in one call. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

	/** Returns every gate of the previous sector to the pool. */
	virtual void Clear(UWorld& World) override;

private:
	/** Gates currently placed in the level by this step. */
	TArray<TWeakObjectPtr<AJumpGate>> ActiveGates;
};
