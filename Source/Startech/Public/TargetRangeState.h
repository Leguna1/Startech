#pragma once

#include "CoreMinimal.h"
#include "TargetRangeState.generated.h"

/** Where the player stands relative to the current target's weapon range and their own. */
UENUM(BlueprintType)
enum class ETargetRangeState : uint8
{
	/** Neither side can hit the other, or there is no target. */
	OutOfRange,
	/** The player can hit the target, but the target can't hit the player. */
	PlayerAdvantage,
	/** The target can hit the player. */
	InEnemyRange
};
