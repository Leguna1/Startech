#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatTestPilot.generated.h"

class AActor;
class UEnemyChargeComponent;
class AStartechEnemyShip;
class AStartechShipPawn;

/** How the test pilot flies a fight (Startech.Test.PilotStyle). */
enum class EPilotStyle : uint8
{
	/** Holds the fight distance with the throttle; no strafing. */
	Face,
	/** Holds the fight distance and strafes all the time, swapping direction every few seconds. */
	Circle,
	/** Reverses away at a set throttle the whole time, whatever the distance. */
	BackOff,
	/** Closes to the fight distance once per target, then never touches the throttle: a target that keeps its course. */
	Hold
};

/**
 * Test aid (non-shipping): flies the player ship in combat so a play session can run unattended. Started with
 * Startech.Test.AutoFight. Each tick it hard-locks the nearest living enemy of the wanted type (the pawn's combat stance
 * then faces it), holds the trigger while the enemy is in laser range, and throttles to keep FightDistance. Beyond lock
 * range it jumps the ship to just outside the fight distance (getting there is not what a session measures). Stops
 * when the time is up or the player dies.
 */
UCLASS()
class UCombatTestPilot : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Game worlds of non-shipping builds only. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	/**
	 * Starts fighting enemies of Type ("Chaser", or "Any") for Seconds, keeping DistanceCm, in Sector (None: the
	 * current one). It goes back there itself after a respawn, or if it starts elsewhere.
	 */
	void Start(const FString& Type, float Seconds, float DistanceCm, FName Sector = NAME_None);

	/** Releases the trigger, the throttle, the strafe and the lock. */
	void Stop(const TCHAR* Reason);

	/**
	 * How later fights are flown. With bDodge, a charged-shot telegraph aimed at the player makes it change its strafe
	 * after DodgeReaction seconds: a circling pilot flips direction, any other starts strafing for a while.
	 * BackOffThrottle (0 to 1) is the reverse throttle of the BackOff style.
	 */
	void SetStyle(EPilotStyle InStyle, bool bInDodge, float InBackOffThrottle);

private:
	/** The nearest living enemy of the wanted type, or null. */
	AStartechEnemyShip* FindTarget(const AActor& Ship) const;

	void HandleEnemyDied(AStartechEnemyShip& Enemy, AActor* Killer);
	void HandleChargeStarted(const UEnemyChargeComponent& Charge);

	/** The steer axis this tick: the strafe of the style, and any dodge. */
	float UpdateSteer(float DeltaTime);

	EPilotStyle Style = EPilotStyle::Face;
	bool bDodge = false;
	float BackOffThrottle = 1.f;
	float StrafeDirection = 1.f;
	float StrafeSwapIn = 0.f;

	/**
	 * After a death, gets back to the fight: true while it is still on the way (respawning, undocking at the station,
	 * warping back to FightSector), when the fight logic is skipped.
	 */
	bool ReturnToFight(AStartechShipPawn& Ship, float DeltaTime);

	/** The sector the fight started in; the pilot warps back there after a respawn. */
	FName FightSector;
	int32 Deaths = 0;

	/** Hold style: the current target has been reached, so the throttle stays at 0. */
	bool bHolding = false;
	bool bWasDead = false;

	/** Seconds until the next warp attempt (a warp is refused for a few seconds after taking damage). */
	float WarpRetryIn = 0.f;

	/** Seconds until a pending dodge happens (negative: none pending), and seconds of dodge strafe left. */
	float DodgeIn = -1.f;
	float DodgeLeft = 0.f;
	int32 Dodges = 0;
	FDelegateHandle ChargeHandle;

	bool bActive = false;
	FString WantedType;
	float Remaining = 0.f;
	float FightDistance = 6000.f;
	int32 Kills = 0;

	/** Seconds until the next status line. */
	float StatusIn = 0.f;

	/** Logs what the pilot is doing every few seconds. */
	void LogStatus(const AStartechShipPawn& Ship, const AStartechEnemyShip* Enemy, const TCHAR* Doing);

	TWeakObjectPtr<AStartechEnemyShip> Target;
	TWeakObjectPtr<AStartechShipPawn> Pilot;
	FDelegateHandle DeathHandle;
};
