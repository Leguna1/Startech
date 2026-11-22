#pragma once

#include "CoreMinimal.h"

/** Text formatting shared by every HUD readout, so distances and countdowns read the same everywhere. */
class STARTECH_API FHudFormat
{
public:
	/** A distance given in cm, as "850 m" below one kilometre and "1.2 km" from there on. */
	static FText Distance(float Centimetres);

	/** A countdown in seconds, as "m:ss", rounded up so it reads 0:00 only once time is really up. */
	static FText Countdown(float Seconds);
};
