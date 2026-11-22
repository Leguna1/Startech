#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Shared answer to "where is the player looking from?", used by everything that is
 * anchored to the camera like a skybox (the near planet and the distant sky bodies).
 */
class PROCSECTOR_API FViewAnchor
{
public:
	/** Returns the first player's camera location, or the level origin when there is no player camera (for example in the editor). */
	static FVector GetViewLocation(const UWorld* World);
};
