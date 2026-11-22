#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatTestPilot.generated.h"

class AActor;
class AStartechEnemyShip;
class AStartechShipPawn;

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

	/** Starts fighting enemies of Type ("Chaser", or "Any") for Seconds, keeping DistanceCm. */
	void Start(const FString& Type, float Seconds, float DistanceCm);

	/** Releases the trigger, the throttle and the lock. */
	void Stop(const TCHAR* Reason);

private:
	/** The nearest living enemy of the wanted type, or null. */
	AStartechEnemyShip* FindTarget(const AActor& Ship) const;

	void HandleEnemyDied(AStartechEnemyShip& Enemy, AActor* Killer);

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
