#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Death/ShipDeathTypes.h"
#include "TeamInterface.h"
#include "StartechShipBase.generated.h"

class AStation;
class UStaticMeshComponent;
class UHealthComponent;
class UWeaponSystemComponent;
class ULaserMountComponent;
class UShieldDrainMountComponent;
class UShipDeathComponent;
class UWeaponEffectsComponent;

// Shared flight model for every ship. Driven through the input setters,
// by player input or by AI.
UCLASS(Abstract)
class STARTECH_API AStartechShipBase : public APawn, public ITeamInterface
{
	GENERATED_BODY()

public:
	AStartechShipBase();

	virtual ETeam GetTeam() const override { return Team; }

	virtual void Tick(float DeltaSeconds) override;

	// Each is clamped to [-1, 1].
	void SetThrottleInput(float Value);
	void SetTurnInput(float Value);
	void SetVerticalInput(float Value);
	void SetStrafeInput(float Value);

	// While set, the ship yaws toward this point at up to TurnRateDegPerSec and ignores the turn intent.
	// Refresh it every tick to follow a moving target.
	void SetFacingLocation(const FVector& Location, float TurnRateDegPerSec);
	void ClearFacingLocation();
	bool HasFacingLocation() const { return bHasFacingLocation; }

	const FVector& GetShipVelocity() const { return ShipVelocity; }

	/** Current speed in cm/s. Cheap; the HUD reads it every frame, like the distance to the target. */
	UFUNCTION(BlueprintPure, Category = "Flight")
	float GetSpeed() const { return static_cast<float>(ShipVelocity.Size()); }

	float GetMaxForwardSpeed() const { return MaxForwardSpeed; }

	/** Acceleration (and the deceleration under reverse throttle), cm/s², before the flight scales. */
	float GetAcceleration() const { return Acceleration; }

	/** The forward top speed without boost: MaxForwardSpeed × the normal speed scale (Engines upgrade). */
	float GetNormalMaxForwardSpeed() const { return MaxForwardSpeed * NormalSpeedScale; }

	/**
	 * Player-only flight scales; enemies never set them and stay at 1.
	 *   NormalScale  every top speed and the acceleration (the Engines upgrade)
	 *   BoostScale   the forward top speed and the acceleration, on top (UShipBoostComponent's multiplier)
	 */
	void SetFlightScales(float NormalScale, float BoostScale);

	/** The normal scale last set with SetFlightScales (the Engines upgrade); 1 for enemies. */
	float GetNormalSpeedScale() const { return NormalSpeedScale; }

	/** The boost scale last set with SetFlightScales; 1 for enemies. */
	float GetBoostSpeedScale() const { return BoostSpeedScale; }

	// Called by the station when the ship enters or leaves its zone; null on leaving.
	// A docked ship takes no damage.
	virtual void SetDockedStation(AStation* Station);
	AStation* GetDockedStation() const { return DockedStation.Get(); }
	bool IsDocked() const { return DockedStation.IsValid(); }

	// The longest Range among this ship's laser mounts that have a weapon, in cm. 0 if none do.
	float GetLaserRange() const;

	UHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UWeaponSystemComponent* GetWeaponSystem() const { return WeaponSystem; }
	ULaserMountComponent* GetLaserMountLeft() const { return LaserMountLeft; }
	ULaserMountComponent* GetLaserMountRight() const { return LaserMountRight; }
	UShieldDrainMountComponent* GetShieldDrainMount() const { return ShieldDrainMount; }
	UShipDeathComponent* GetShipDeath() const { return ShipDeath; }

	/** This ship type's death effects, over the settings' defaults. The base returns DeathEffects. */
	virtual const FShipDeathEffects& GetDeathEffects() const { return DeathEffects; }

protected:
	virtual void BeginPlay() override;

	// How far the ship leans into turns. Subclasses can lower it, e.g. in a cockpit view.
	virtual float GetBankLimit() const;

	FVector GetLevelForward() const;

	// Stops the ship dead: no velocity, no yaw speed, no intents, no facing point.
	void ResetFlightState();

	// Root, so its collision is what movement sweeps against.
	UPROPERTY(VisibleAnywhere, Category = "Ship")
	TObjectPtr<UStaticMeshComponent> ShipMesh;

	// Ships on the same non-Neutral team cannot damage each other.
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	ETeam Team = ETeam::Neutral;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UWeaponSystemComponent> WeaponSystem;

	// Mount positions are the muzzles. Place them on the hull in the Blueprint.
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<ULaserMountComponent> LaserMountLeft;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<ULaserMountComponent> LaserMountRight;

	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UShieldDrainMountComponent> ShieldDrainMount;

	// Spawns the mounts' effects and sounds from their weapon data.
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UWeaponEffectsComponent> WeaponEffects;

	// The dying state between hull 0 and removal: drift, tumble, flash, explosions. See UShipDeathComponent.
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	TObjectPtr<UShipDeathComponent> ShipDeath;

	// Death effect hooks for this ship type. Empty fields use UStartechSettings::Death.DefaultEffects.
	// Enemies use their UEnemyData's instead.
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FShipDeathEffects DeathEffects;

	// Speeds in cm/s, acceleration in cm/s^2, turn rate in deg/s.
	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxForwardSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxReverseSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxStrafeSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxVerticalSpeed = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float Acceleration = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float Braking = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxTurnRate = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float TurnResponse = 4.f;

	// Yaw speed limit while turning to face a facing location.
	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float FacingTurnRate = 120.f;

	// While facing a point, the yaw rate is the remaining angle times this (1/s), up to the caller's turn rate.
	// It eases to zero as the nose arrives, so a moving facing point is tracked smoothly.
	UPROPERTY(EditDefaultsOnly, Category = "Flight", meta = (ClampMin = "0"))
	float FacingResponse = 10.f;

	// How fast sideways speed dies off with no strafe input. High, so free flight doesn't drift sideways.
	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float LateralBraking = 8000.f;

	// Visual lean into turns. The chase camera ignores it, so its horizon stays level.
	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxBankAngle = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float BankResponse = 3.f;

	// Visual nose tilt when climbing or descending while moving forward.
	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float MaxPitchAngle = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	float PitchResponse = 3.f;

	// Everything that counts as the death, at hull 0: credit the kill, stop the ship. The base version makes the ship
	// untargetable (which releases any lock on it) and takes it off the radar. Overrides must call Super. The dying
	// state starts after it returns.
	virtual void HandleDeath(AActor* Killer);

	// The end of the dying state: the final explosion went off at Location and the ship is hidden. Drop loot and
	// leave play here. The base version does nothing.
	virtual void HandleDyingFinished(const FVector& Location);

private:
	// Bound to the health component's OnDeath: HandleDeath, then the dying state.
	UFUNCTION()
	void HandleHealthDeath(AActor* Killer);

	void UpdateTurn(float DeltaSeconds);
	void UpdateVelocity(float DeltaSeconds);
	void MoveShip(float DeltaSeconds);

	// This frame's yaw change toward the facing location: proportional to the remaining angle,
	// limited by the caller's turn rate, and never past the target heading.
	float GetFacingYawStep(float DeltaSeconds) const;

	float ThrottleInput = 0.f;
	float TurnInput = 0.f;
	float VerticalInput = 0.f;
	float StrafeInput = 0.f;

	FVector FacingLocation = FVector::ZeroVector;
	float FacingTurnRateDegPerSec = 0.f;
	bool bHasFacingLocation = false;

	/** See SetFlightScales. */
	float NormalSpeedScale = 1.f;

	/** See SetFlightScales. */
	float BoostSpeedScale = 1.f;

	FVector ShipVelocity = FVector::ZeroVector;
	float YawSpeed = 0.f;
	float BankAngle = 0.f;
	float PitchAngle = 0.f;

	TWeakObjectPtr<AStation> DockedStation;
};
