#pragma once

#include "CoreMinimal.h"
#include "MapMarkerType.generated.h"

// What a map marker stands for. The radar and the screen markers pick their style by it.
UENUM(BlueprintType)
enum class EMapMarkerType : uint8
{
	Station,
	Enemy,
	Loot,
	DroppedCargo,
	Objective,
	/** A jump gate. Its marker is added at runtime by UGateMarkerSubsystem. */
	Gate
};
