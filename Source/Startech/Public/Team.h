#pragma once

#include "CoreMinimal.h"
#include "Team.generated.h"

UENUM(BlueprintType)
enum class ETeam : uint8
{
	// Never counts as friendly, not even to other Neutrals.
	Neutral,
	Player,
	Enemy
};
