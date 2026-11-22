#include "Factories/EnvironmentParamsFactory.h"

#include "Data/EnvironmentPreset.h"
#include "Math/RandomStream.h"

FEnvironmentParams FEnvironmentParamsFactory::Build(const UEnvironmentPreset& Preset, const int32 Seed)
{
	FEnvironmentParams Params = Preset.BaseParams;
	FRandomStream Stream(Seed);

	const float Hue = Preset.HueJitterDegrees;
	const float Fraction = Preset.ScalarJitterFraction;

	// The order of the calls below must never change: reordering them changes every existing sector.
	Params.Planet.SurfaceColorA = JitterHue(Params.Planet.SurfaceColorA, Hue, Stream);
	Params.Planet.SurfaceColorB = JitterHue(Params.Planet.SurfaceColorB, Hue, Stream);
	Params.Planet.Radius = JitterScalar(Params.Planet.Radius, Fraction, Stream);
	Params.Planet.CloudCoverage = FMath::Clamp(JitterScalar(Params.Planet.CloudCoverage, Fraction, Stream), 0.0f, 1.0f);
	Params.Planet.NoiseScale = JitterScalar(Params.Planet.NoiseScale, Fraction, Stream);
	Params.Planet.SpinDegreesPerSecond = JitterScalar(Params.Planet.SpinDegreesPerSecond, Fraction, Stream);

	Params.Atmosphere.Color = JitterHue(Params.Atmosphere.Color, Hue, Stream);
	Params.Atmosphere.Intensity = JitterScalar(Params.Atmosphere.Intensity, Fraction, Stream);

	Params.Sun.Color = JitterHue(Params.Sun.Color, Hue * 0.5f, Stream);
	Params.Sun.Intensity = JitterScalar(Params.Sun.Intensity, Fraction, Stream);

	Params.Ambient.SkyLightColor = JitterHue(Params.Ambient.SkyLightColor, Hue, Stream);
	Params.Ambient.FogColor = JitterHue(Params.Ambient.FogColor, Hue, Stream);

	Params.Backdrop.NebulaColor = JitterHue(Params.Backdrop.NebulaColor, Hue, Stream);
	Params.Backdrop.NebulaIntensity = JitterScalar(Params.Backdrop.NebulaIntensity, Fraction, Stream);
	Params.Backdrop.StarDensity = FMath::Clamp(JitterScalar(Params.Backdrop.StarDensity, Fraction, Stream), 0.0f, 1.0f);

	Params.Asteroids.Count = FMath::Max(0, FMath::RoundToInt(JitterScalar(static_cast<float>(Params.Asteroids.Count), Fraction, Stream)));

	if (Preset.bRandomizeSunAndSky)
	{
		Params.Sun.Rotation = FRotator(Stream.FRandRange(-60.0f, -10.0f), Stream.FRandRange(0.0f, 360.0f), 0.0);
		Params.Backdrop.Rotation = FRotator(Stream.FRandRange(0.0f, 360.0f), Stream.FRandRange(0.0f, 360.0f), Stream.FRandRange(0.0f, 360.0f));

		// Swing the planet around the level so it sits in a different part of the sky, at the same distance.
		Params.Planet.Location = Params.Planet.Location.RotateAngleAxis(Stream.FRandRange(0.0f, 360.0f), FVector::UpVector);
	}

	// Added after the first release, so it draws last and leaves every earlier value unchanged.
	Params.Backdrop.NebulaColorB = JitterHue(Params.Backdrop.NebulaColorB, Hue, Stream);

	// The visible sun disc always sits where the sunlight comes from. A star system, if used, overrides both later.
	Params.Backdrop.SunDirection = -Params.Sun.Rotation.Vector();
	Params.Backdrop.SunColor = Params.Sun.Color;

	return Params;
}

void FEnvironmentParamsFactory::ResolvePlanetshine(FEnvironmentParams& Params)
{
	FAmbientParams& Ambient = Params.Ambient;
	const FPlanetParams& Planet = Params.Planet;

	const FVector ToPlanet = Planet.Location.GetSafeNormal();
	if (!Planet.bVisible || ToPlanet.IsNearlyZero() || Ambient.PlanetshineStrength <= 0.0f)
	{
		Ambient.PlanetshineIntensity = 0.0f;
		return;
	}

	// Light leaves the planet and travels toward the player.
	Ambient.PlanetshineRotation = (-ToPlanet).Rotation();

	// Average the two surface colours, then normalise so brightness comes only from the intensity.
	const FLinearColor Surface = (Planet.SurfaceColorA + Planet.SurfaceColorB) * 0.5f;
	const float Brightest = FMath::Max3(Surface.R, Surface.G, Surface.B);
	Ambient.PlanetshineColor = Brightest > UE_KINDA_SMALL_NUMBER ? Surface / Brightest : FLinearColor::White;
	Ambient.PlanetshineColor.A = 1.0f;

	// Phase: 1 when the sun is behind the player (full day side visible), 0 when it is behind the planet.
	const FVector ToSun = (-Params.Sun.Rotation.Vector()).GetSafeNormal();
	const float Phase = (1.0f - static_cast<float>(FVector::DotProduct(ToPlanet, ToSun))) * 0.5f;
	const float PhaseFactor = FMath::Lerp(Ambient.PlanetshineMinimumPhase, 1.0f, Phase);

	Ambient.PlanetshineIntensity = Params.Sun.Intensity * Ambient.PlanetshineStrength * PhaseFactor;
}

FLinearColor FEnvironmentParamsFactory::JitterHue(const FLinearColor& Color, const float MaxDegrees, FRandomStream& Stream)
{
	// Always consume one random number so the stream stays aligned even when jitter is zero.
	const float Shift = Stream.FRandRange(-1.0f, 1.0f) * MaxDegrees;

	FLinearColor Hsv = Color.LinearRGBToHSV();
	Hsv.R = FMath::Fmod(Hsv.R + Shift + 360.0f, 360.0f);

	FLinearColor Result = Hsv.HSVToLinearRGB();
	Result.A = Color.A;
	return Result;
}

float FEnvironmentParamsFactory::JitterScalar(const float Value, const float Fraction, FRandomStream& Stream)
{
	return Value * (1.0f + Stream.FRandRange(-1.0f, 1.0f) * Fraction);
}
