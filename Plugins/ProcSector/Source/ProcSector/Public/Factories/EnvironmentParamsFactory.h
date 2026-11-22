#pragma once

#include "CoreMinimal.h"
#include "Data/SectorTypes.h"

class UEnvironmentPreset;
struct FRandomStream;

/**
 * Factory that turns a preset plus a seed into the final environment parameters.
 * It is the only place where sector randomness for the look of a destination is decided,
 * and it is deterministic: the same preset and seed always give the same result.
 */
class PROCSECTOR_API FEnvironmentParamsFactory
{
public:
	/** Builds the parameters for one sector from its preset and seed. */
	static FEnvironmentParams Build(const UEnvironmentPreset& Preset, const int32 Seed);

	/**
	 * Works out the light reflected off the near planet from the final sun and planet values:
	 * its direction (from the planet toward the player), colour (the planet's surface) and intensity
	 * (strength times how much of the planet's day side faces the player). Call it last, after
	 * anything that can still move the sun or the planet, such as FStarSystemSkyFactory.
	 */
	static void ResolvePlanetshine(FEnvironmentParams& Params);

private:
	/** Shifts a colour's hue by up to MaxDegrees in either direction, keeping saturation, value and alpha. */
	static FLinearColor JitterHue(const FLinearColor& Color, const float MaxDegrees, FRandomStream& Stream);

	/** Scales a value by a random factor between (1 - Fraction) and (1 + Fraction). */
	static float JitterScalar(const float Value, const float Fraction, FRandomStream& Stream);
};
