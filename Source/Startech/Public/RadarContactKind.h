#pragma once

#include "CoreMinimal.h"
#include "RadarContactKind.generated.h"

/** How the radar classes a contact, derived from its map marker type. */
UENUM(BlueprintType)
enum class ERadarContactKind : uint8
{
	/** An enemy ship. */
	Hostile,
	/** Loot or dropped cargo. */
	Loot,
	/** Everything else: stations, gates, objectives. */
	Neutral
};
