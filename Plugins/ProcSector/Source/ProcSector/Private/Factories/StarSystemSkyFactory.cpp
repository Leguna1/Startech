#include "Factories/StarSystemSkyFactory.h"

bool FStarSystemSkyFactory::Apply(const FStarSystemDefinition& System, const FName HomeBodyId, FEnvironmentParams& Params)
{
	const FCelestialBodyDefinition* Home = System.Bodies.FindByPredicate([HomeBodyId](const FCelestialBodyDefinition& Body)
	{
		return Body.BodyId == HomeBodyId;
	});

	if (Home == nullptr || HomeBodyId.IsNone())
	{
		return false;
	}

	const FVector HomePosition = GetBodyPosition(*Home);
	const float OrbitRadius = FMath::Max(Home->OrbitRadius, 0.01f);

	// The sun sits at the origin of the system, so it lies in the opposite direction of our own position.
	FVector ToSun = (-HomePosition).GetSafeNormal();
	if (ToSun.IsNearlyZero())
	{
		ToSun = FVector::ForwardVector;
	}

	// Light travels from the sun toward the viewer, so every body's lit side faces the visible sun.
	Params.Sun.Rotation = (-ToSun).Rotation();
	Params.Sun.Color = System.SunColor;
	Params.Sun.Intensity *= FMath::Pow(OrbitRadius, -System.SunFalloffExponent);

	Params.Backdrop.SunDirection = ToSun;
	Params.Backdrop.SunColor = System.SunColor;
	Params.Backdrop.SunAngularRadiusDegrees = FMath::Clamp(System.SunAngularRadiusAtUnitOrbit / OrbitRadius, 0.15f, 10.0f);

	Params.SkyBodies.Reset();
	AddNeighbours(System, *Home, HomePosition, Params.SkyBodies);
	AddMoons(*Home, Params.Planet, Params.SkyBodies);
	return true;
}

FVector FStarSystemSkyFactory::GetBodyPosition(const FCelestialBodyDefinition& Body)
{
	const float AngleRadians = FMath::DegreesToRadians(Body.OrbitAngleDegrees);
	return FVector(Body.OrbitRadius * FMath::Cos(AngleRadians), Body.OrbitRadius * FMath::Sin(AngleRadians), Body.OrbitHeight);
}

void FStarSystemSkyFactory::AddNeighbours(const FStarSystemDefinition& System, const FCelestialBodyDefinition& Home, const FVector& HomePosition, TArray<FSkyBodyParams>& OutBodies)
{
	const float MaxAngular = FMath::Max(System.MaxBodyAngularRadius, System.MinBodyAngularRadius);

	for (const FCelestialBodyDefinition& Body : System.Bodies)
	{
		if (Body.BodyId == Home.BodyId)
		{
			continue;
		}

		const FVector Offset = GetBodyPosition(Body) - HomePosition;
		const double Distance = Offset.Size();
		if (Distance < UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FSkyBodyParams& SkyBody = OutBodies.AddDefaulted_GetRef();
		SkyBody.BodyId = Body.BodyId;
		SkyBody.Direction = Offset / Distance;
		SkyBody.AngularRadiusDegrees = FMath::Clamp(
			Body.BodyRadius * System.BodyAngularRadiusAtUnitDistance / static_cast<float>(Distance),
			System.MinBodyAngularRadius, MaxAngular);
		SkyBody.ColorA = Body.ColorA;
		SkyBody.ColorB = Body.ColorB;
	}
}

void FStarSystemSkyFactory::AddMoons(const FCelestialBodyDefinition& Home, const FPlanetParams& Planet, TArray<FSkyBodyParams>& OutBodies)
{
	if (!Planet.bVisible || Home.Moons.IsEmpty())
	{
		return;
	}

	const double PlanetDistance = Planet.Location.Size();
	if (PlanetDistance < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FRotator PlanetRotation = Planet.Location.Rotation();
	const double SineOfHalfAngle = FMath::Clamp(static_cast<double>(Planet.Radius) / PlanetDistance, 0.0, 1.0);
	const float PlanetAngularRadius = FMath::RadiansToDegrees(static_cast<float>(FMath::Asin(SineOfHalfAngle)));

	for (const FMoonDefinition& Moon : Home.Moons)
	{
		FSkyBodyParams& SkyBody = OutBodies.AddDefaulted_GetRef();
		SkyBody.BodyId = Moon.MoonId;
		SkyBody.Direction = (PlanetRotation + Moon.OffsetFromPlanet).Vector();
		SkyBody.AngularRadiusDegrees = PlanetAngularRadius * Moon.RelativeRadius;
		SkyBody.ColorA = Moon.ColorA;
		SkyBody.ColorB = Moon.ColorB;
	}
}
