#pragma once

#include "CoreMinimal.h"
#include "TargetLockState.generated.h"

UENUM(BlueprintType)
enum class ETargetLockState : uint8
{
	None,
	// Picked automatically from what is near the centre of view; changes as you look around.
	Soft,
	// Pinned by the player; held until released or broken.
	Hard
};
