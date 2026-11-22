#pragma once

#include "CoreMinimal.h"
#include "Data/SectorTypes.h"

/**
 * Factory that works out what the sky looks like from one planet of a star system:
 * where the sun is and how big it looks, which other planets are visible and where,
 * and where the home planet's moons sit. Directions are true to the system's layout;
 * sizes are deliberately exaggerated so neighbours read as planets rather than dots.
 */
class PROCSECTOR_API FStarSystemSkyFactory
{
public:
	/**
	 * Rewrites the sun, the sun disc and the sky bodies of Params for a viewer next to HomeBodyId.
	 * Returns false and changes nothing when the system has no body with that id.
	 */
	static bool Apply(const FStarSystemDefinition& System, const FName HomeBodyId, FEnvironmentParams& Params);

private:
	/** Position of a body in orbit units, with the sun at the origin and orbits in the horizontal plane. */
	static FVector GetBodyPosition(const FCelestialBodyDefinition& Body);

	/** Adds one sky body per other planet of the system, as seen from HomePosition. */
	static void AddNeighbours(const FStarSystemDefinition& System, const FCelestialBodyDefinition& Home, const FVector& HomePosition, TArray<FSkyBodyParams>& OutBodies);

	/** Adds the home planet's moons, placed relative to where the near planet appears. */
	static void AddMoons(const FCelestialBodyDefinition& Home, const FPlanetParams& Planet, TArray<FSkyBodyParams>& OutBodies);
};
