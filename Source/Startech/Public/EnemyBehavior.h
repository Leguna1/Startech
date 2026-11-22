#pragma once

#include "CoreMinimal.h"
#include "EnemyBehavior.generated.h"

UENUM(BlueprintType)
enum class EEnemyBehavior : uint8
{
	// Wanders near home; fights back only when the player damages it.
	Passive,
	// Holds position, engages on sight, closes to PreferredRange.
	Chaser,
	// Holds position, engages on sight, keeps PreferredRange and strafes.
	Ranged
};
