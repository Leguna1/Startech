#pragma once

#include "CoreMinimal.h"
#include "ShipBoostSettings.generated.h"

/** Every number of the player's boost (UShipBoostComponent) and its camera feel. In UStartechSettings::Boost. */
USTRUCT(BlueprintType)
struct STARTECH_API FShipBoostSettings
{
	GENERATED_BODY()

	/** The boost energy pool when full, before upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "1"))
	float MaxBoostEnergy = 100.f;

	/** Energy used per second while boosting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float DrainPerSecond = 25.f;

	/** Energy regained per second once regeneration has started, before upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0"))
	float RegenPerSecond = 20.f;

	/** Seconds after boosting stops before energy regenerates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0", Units = "s"))
	float RegenDelaySeconds = 1.5f;

	/** After the pool runs empty, boosting can't start again until energy is above this fraction of the maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy", meta = (ClampMin = "0", ClampMax = "1"))
	float RestartFraction = 0.25f;

	/** The top speed and acceleration multiplier at full boost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "1"))
	float MaxSpeedMultiplier = 2.2f;

	/** Seconds for the multiplier to climb from 1 to MaxSpeedMultiplier while boosting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.01", Units = "s"))
	float RampUpSeconds = 1.5f;

	/** Seconds for the multiplier to fall from MaxSpeedMultiplier back to 1 once boosting stops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speed", meta = (ClampMin = "0.01", Units = "s"))
	float RampDownSeconds = 0.8f;

	/** Shortest time between two OnBoostEnergyChanged broadcasts (0.1 s: at most 10 a second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0", Units = "s"))
	float EnergyEventInterval = 0.1f;

	/** Degrees added to the camera's field of view at full boost (chase and cockpit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0"))
	float FieldOfViewWidening = 8.f;

	/**
	 * The chase camera has no position lag outside a boost. Lag starts at this speed as the boost begins (barely
	 * visible) and moves toward CameraLagSpeedAtFullBoost. Higher is less lag: at a steady speed the camera trails by
	 * about speed / lag speed, so 100 at 3000 cm/s is 30 cm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraLagSpeedAtStart = 100.f;

	/** The chase camera's position lag speed at full boost: 30 trails about 2 m at 6600 cm/s, a slight drop back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraLagSpeedAtFullBoost = 30.f;
};
