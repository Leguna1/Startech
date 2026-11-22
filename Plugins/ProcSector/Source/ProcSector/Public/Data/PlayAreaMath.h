#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

/**
 * Converts play-area fractions into world positions. A sector's playable space is a box centred on the
 * level origin with half-size FSectorDefinition::PlayAreaExtent; everything the plugin places is authored
 * in unit space (each axis -1 to 1) and multiplied per axis by that extent, so one edit resizes a sector.
 * Header-only so the asteroid field and the spawn planner share exactly the same maths.
 */
struct FPlayAreaMath
{
	/** Converts a unit-space position (each axis -1 to 1) to a world position by multiplying each axis by the extent. */
	static FVector ToWorld(const FVector& Normalized, const FVector& Extent)
	{
		return Normalized * Extent;
	}

	/**
	 * Picks a random point in a flat ring around the origin, evenly distributed over the ring's area,
	 * and returns it in world space. The fractions are clamped to 0 to 1 and OuterFraction is raised to
	 * InnerFraction when smaller. A non-square extent turns the ring into an ellipse that still fits the box.
	 * Draws exactly three values from the stream, in this order: radius blend, angle, height.
	 */
	static FVector RandomPointInRing(FRandomStream& Stream, const float InnerFraction, const float OuterFraction, const float HeightFraction, const FVector& Extent)
	{
		const float Inner = FMath::Clamp(InnerFraction, 0.0f, 1.0f);
		const float Outer = FMath::Max(FMath::Clamp(OuterFraction, 0.0f, 1.0f), Inner);
		const float Height = FMath::Clamp(HeightFraction, 0.0f, 1.0f);

		// Square root keeps the density even across the ring instead of clustering near the inner edge.
		const float Radius = FMath::Sqrt(Stream.FRandRange(FMath::Square(Inner), FMath::Square(Outer)));
		const float Angle = Stream.FRandRange(0.0f, UE_TWO_PI);
		const float UnitHeight = Stream.FRandRange(-Height, Height);

		return ToWorld(FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), UnitHeight), Extent);
	}

	/**
	 * True when the world position lies in the ring that RandomPointInRing draws from, with the same unit-space maths
	 * and the same clamping of the fractions: horizontal unit radius between Inner and Outer, unit height within
	 * HeightFraction. An axis with zero extent only accepts a coordinate of zero.
	 */
	static bool IsInsideRing(const FVector& Location, const float InnerFraction, const float OuterFraction, const float HeightFraction, const FVector& Extent)
	{
		constexpr float Tolerance = 1.0e-4f;

		const float Inner = FMath::Clamp(InnerFraction, 0.0f, 1.0f);
		const float Outer = FMath::Max(FMath::Clamp(OuterFraction, 0.0f, 1.0f), Inner);
		const float Height = FMath::Clamp(HeightFraction, 0.0f, 1.0f);

		FVector Unit;
		if (!ToUnit(Location, Extent, Unit))
		{
			return false;
		}

		const double Radius = FVector2D(Unit.X, Unit.Y).Size();
		return Radius >= Inner - Tolerance && Radius <= Outer + Tolerance && FMath::Abs(Unit.Z) <= Height + Tolerance;
	}

	/** True when the world position lies inside the play-area box (each axis within plus or minus the extent). */
	static bool IsInsidePlayArea(const FVector& Location, const FVector& Extent)
	{
		return FMath::Abs(Location.X) <= Extent.X && FMath::Abs(Location.Y) <= Extent.Y && FMath::Abs(Location.Z) <= Extent.Z;
	}

	/**
	 * Converts a world position to unit space by dividing each axis by the extent; the inverse of ToWorld.
	 * An axis with zero extent maps to 0 when the coordinate is zero; otherwise returns false.
	 */
	static bool ToUnit(const FVector& Location, const FVector& Extent, FVector& OutUnit)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (FMath::IsNearlyZero(Extent[Axis]))
			{
				if (!FMath::IsNearlyZero(Location[Axis], 1.0))
				{
					return false;
				}
				OutUnit[Axis] = 0.0;
			}
			else
			{
				OutUnit[Axis] = Location[Axis] / Extent[Axis];
			}
		}
		return true;
	}
};
