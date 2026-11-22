#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

class USectorGenStep;

/**
 * Factory for the generation pipeline. The director asks it for step objects and never
 * names a concrete step class, so steps can be added, removed or reordered from the
 * project settings without touching the director.
 */
class PROCSECTOR_API FSectorGenStepFactory
{
public:
	/**
	 * Creates one step object per class, in order, owned by Outer.
	 * An empty list produces the built-in pipeline from GetDefaultStepClasses.
	 */
	static TArray<USectorGenStep*> CreatePipeline(UObject* Outer, const TArray<TSoftClassPtr<USectorGenStep>>& StepClasses);

	/** Fills OutClasses with the built-in steps in their intended order. */
	static void GetDefaultStepClasses(TArray<TSubclassOf<USectorGenStep>>& OutClasses);
};
