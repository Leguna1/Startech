#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipBoostComponent.generated.h"

struct FShipBoostSettings;

/** Broadcast when the boost energy or its maximum changes; at most once per FShipBoostSettings::EnergyEventInterval. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBoostEnergyChanged, float, Current, float, Max);

/** Broadcast when boosting starts (true) or stops (false). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoostStateChanged, bool, bBoosting);

/** Broadcast when the boost key is pressed but boosting can't start: the pool is still below the restart threshold. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBoostDenied);

/**
 * The player's boost: an energy pool spent while the boost key is held, and a speed multiplier that climbs while
 * boosting and eases back after. The owning ship drives it from its own tick (UpdateBoost), passes the key presses
 * (SetBoostHeld), and applies GetSpeedMultiplier to its flight model and GetBoostAlpha to its camera.
 *
 * Rules (numbers in UStartechSettings::Boost):
 *   - boosting drains energy; the multiplier ramps from 1 to MaxSpeedMultiplier over RampUpSeconds
 *   - on release or an empty pool it stops; the multiplier falls back to 1 over RampDownSeconds
 *   - after emptying the pool, boosting needs a new press with energy above RestartFraction (else OnBoostDenied)
 *   - energy regenerates RegenDelaySeconds after boosting stops
 */
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UShipBoostComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Ticked by the owner, not on its own. */
	UShipBoostComponent();

	/**
	 * Advances the boost by DeltaSeconds: drain or regeneration, the multiplier ramp, and the events. bAllowed false
	 * (hub, pause, warp, death) stops any boost as a release would.
	 */
	void UpdateBoost(float DeltaSeconds, bool bAllowed);

	/** The boost key went down (true) or up (false). A press below the restart threshold fires OnBoostDenied. */
	void SetBoostHeld(bool bHeld);

	/** Full energy, no boost, multiplier 1, no lockout: on load and on respawn. Broadcasts the new state. */
	void Refill();

	/** Stops boosting at once with the multiplier back to 1, keeping the energy: on death. */
	void StopImmediately();

	/**
	 * Scales the pool's maximum and the regeneration rate (the Booster upgrade). Raising the maximum raises the
	 * current energy by the same amount.
	 */
	void SetEnergyScales(float MaxEnergyScale, float RegenScale);

	/** Current boost energy. */
	UFUNCTION(BlueprintPure, Category = "Boost")
	float GetEnergy() const { return Energy; }

	/** The pool's maximum, upgrades included. */
	UFUNCTION(BlueprintPure, Category = "Boost")
	float GetMaxEnergy() const;

	/** Energy regenerated per second, upgrades included. */
	float GetRegenPerSecond() const;

	/** Energy needed to start boosting again after the pool ran empty. */
	UFUNCTION(BlueprintPure, Category = "Boost")
	float GetRestartThreshold() const;

	/** True while boosting (held, allowed and with energy). */
	UFUNCTION(BlueprintPure, Category = "Boost")
	bool IsBoosting() const { return bBoosting; }

	/** True from the pool running empty until energy is back above the restart threshold. */
	UFUNCTION(BlueprintPure, Category = "Boost")
	bool IsLockedOut() const { return bLockedOut; }

	/** The multiplier on top speed and acceleration now: 1 at rest, up to MaxSpeedMultiplier. */
	UFUNCTION(BlueprintPure, Category = "Boost")
	float GetSpeedMultiplier() const { return SpeedMultiplier; }

	/** How far into the boost the multiplier is, 0 at rest to 1 at full: drives the camera feel. */
	float GetBoostAlpha() const;

	/** Energy or its maximum changed; throttled to EnergyEventInterval, but the empty and full values always fire. */
	UPROPERTY(BlueprintAssignable, Category = "Boost")
	FOnBoostEnergyChanged OnBoostEnergyChanged;

	/** Boosting started or stopped. */
	UPROPERTY(BlueprintAssignable, Category = "Boost")
	FOnBoostStateChanged OnBoostStateChanged;

	/** A press was refused because the pool is below the restart threshold after running empty. */
	UPROPERTY(BlueprintAssignable, Category = "Boost")
	FOnBoostDenied OnBoostDenied;

protected:
	/** Starts with a full pool. */
	virtual void BeginPlay() override;

private:
	/** The boost numbers from UStartechSettings. */
	static const FShipBoostSettings& GetSettings();

	/** Sets bBoosting and broadcasts OnBoostStateChanged when it changes; stopping starts the regeneration delay. */
	void SetBoosting(bool bNewBoosting);

	/** Broadcasts OnBoostEnergyChanged now if bForce, or once the interval has passed since the last one. */
	void NotifyEnergy(bool bForce);

	/** Current energy. */
	float Energy = 0.f;

	/** The current speed multiplier. */
	float SpeedMultiplier = 1.f;

	/** The boost key is down. */
	bool bHeld = false;

	/** Boosting now. */
	bool bBoosting = false;

	/** The pool ran empty and is not yet back above the restart threshold. */
	bool bLockedOut = false;

	/** Seconds since boosting last stopped, for the regeneration delay. */
	float TimeSinceBoostStopped = 0.f;

	/** Seconds since the last OnBoostEnergyChanged. */
	float TimeSinceEnergyEvent = 0.f;

	/** The energy last broadcast, to skip unchanged values. */
	float LastBroadcastEnergy = -1.f;

	/** The maximum last broadcast. */
	float LastBroadcastMax = -1.f;

	/** Booster upgrade scale on MaxBoostEnergy. */
	float MaxEnergyScale = 1.f;

	/** Booster upgrade scale on RegenPerSecond. */
	float RegenScale = 1.f;
};
