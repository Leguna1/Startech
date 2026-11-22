#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyBrainComponent.generated.h"

class AController;
class AStartechEnemyShip;
class APawn;
class UDamageType;
class UEnemyData;
class UHealthComponent;
struct FEnemyBehaviourBalance;

// Flies an AStartechEnemyShip using only the ship's public intent setters, facing functions
// and weapon system. Its type of behaviour comes from the ship's EnemyData (Passive, Chaser, Ranged) and its
// numbers from the ship's GetBehaviour() (DA_CombatBalance): leashes, circling and the afterburner.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UEnemyBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyBrainComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Stops all thinking, e.g. on death, until ResetBrain. Ends an afterburn.
	void StopBrain();

	/**
	 * Restarts thinking from scratch, undoing StopBrain: idle and calm (no aggro, leash timers, afterburn or cooldown),
	 * no wander point, ticking again, with Home as the point it guards and returns to. Called at the start of each
	 * life (a pooled enemy's new spawn location).
	 */
	void ResetBrain(const FVector& Home);

	/**
	 * The band around the preferred range in which the enemy holds still, as a fraction of it. An engaged enemy can
	 * sit as far out as PreferredRange × (1 + this), so its laser must reach that far (checked by the TTK table).
	 */
	float GetRangeTolerance() const { return RangeTolerance; }

	/** Fighting the player (not idle or on its way home). */
	bool IsEngaged() const { return State == EBrainState::Engage; }

	/** Flying home after giving up. */
	bool IsReturningHome() const { return State == EBrainState::ReturnHome; }

	bool IsAfterburning() const { return bAfterburning; }

	/** Turned away from a player who came too close (FEnemyBehaviourBalance::RetreatFraction); not firing. */
	bool IsRetreating() const { return bRetreating; }

	/** Seconds until it may start the next charged shot (it also needs range and line of sight). */
	float GetChargeWait() const { return ChargeWaitLeft; }

	/** Seconds until the afterburner can start again (0 when ready). */
	float GetAfterburnCooldown() const { return AfterburnCooldownLeft; }

	/** "idle", "engage", "return", or "stopped" (dead, until ResetBrain), for test logs. */
	const TCHAR* GetStateName() const;

	/** Why the last engagement ended: "quiet", "far", "home", "no player" or empty. For test logs. */
	const TCHAR* GetLastGiveUpReason() const { return LastGiveUpReason; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	// Close enough to a destination (wander point or home) to count as arrived, in cm.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float ArrivalRadius = 500.f;

	// Height difference ignored when matching the player's altitude, in cm.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float HeightDeadzone = 150.f;

	// Height difference at which vertical intent reaches full, in cm. Smaller differences ramp down.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "1"))
	float HeightFullIntentDistance = 1000.f;

	// Band around PreferredRange, as a fraction of it, in which throttle is zero.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float RangeTolerance = 0.1f;

	// Strafing enemies (Ranged, and circling ones) swap strafe direction after a random time in this range, in seconds.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float StrafeSwapMinTime = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float StrafeSwapMaxTime = 4.f;

	// A circler beyond its laser range heads straight for the player until this × its laser range, then turns out
	// gradually to a point PreferredRange beside the player, reached at laser range.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "1.01"))
	float FlankBlendStart = 1.5f;

private:
	enum class EBrainState : uint8
	{
		Idle,
		Engage,
		ReturnHome
	};

	void TickIdle(AStartechEnemyShip& Ship, const UEnemyData& Data, const APawn* Player, float DeltaSeconds);
	void TickEngage(AStartechEnemyShip& Ship, const UEnemyData& Data, const APawn& Player, float DeltaSeconds);
	void TickReturnHome(AStartechEnemyShip& Ship, const UEnemyData& Data, float DeltaSeconds);

	void SetState(EBrainState NewState);

	/** Ends the engagement: holds fire, ends any afterburn and flies home. Reason is for the test logs. */
	void GiveUp(AStartechEnemyShip& Ship, const TCHAR* Reason, float DeltaSeconds);

	/** True (and gives up) when a leash has run out: the quiet time, the far time or the home range. */
	bool UpdateLeash(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour, float Distance, float LaserRange,
		const FVector& PlayerLocation, float DeltaSeconds);

	/** Starts a burn when the player is beyond the trigger distance and the cooldown is over; ends it on time or in range. */
	void UpdateAfterburn(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour, float Distance,
		float PredictedDistance, float LaserRange, float DeltaSeconds);

	/**
	 * The distance to the player once this ship has braked, at its acceleration, from its closing speed (relative to
	 * the player's motion) down to the closing speed it would have at SlowToSpeed. With 0: where it would stop if it
	 * reversed now, used to brake before the range band rather than inside it (no overshoot into the player) and to
	 * end an afterburn at the preferred range.
	 */
	static float GetPredictedDistance(const AStartechEnemyShip& Ship, const APawn& Player, float Distance, float SlowToSpeed);

	void StartAfterburn(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour);

	/** Ends a burn (flight scale back to 1). With bStartCooldown, the next one waits AfterburnCooldown. */
	void StopAfterburn(bool bStartCooldown);

	/**
	 * A player inside RetreatFraction × the preferred range turns it away to fly off without firing, until it is back
	 * at the preferred range. True while retreating (it has flown this tick).
	 */
	bool UpdateRetreat(AStartechEnemyShip& Ship, const UEnemyData& Data, const FEnemyBehaviourBalance& Behaviour,
		const FVector& PlayerLocation, float Distance, float DeltaSeconds);

	/**
	 * Counts down to the next charged shot and starts it once the player is in laser range and in sight. True while a
	 * charge is in progress, when normal fire holds.
	 */
	bool UpdateCharge(AStartechEnemyShip& Ship, const FEnemyBehaviourBalance& Behaviour, const APawn& Player, bool bInRange,
		float DeltaSeconds);

	/** Nothing that blocks shots lies between the ship and the player. */
	bool HasLineOfSight(const AStartechEnemyShip& Ship, const APawn& Player) const;

	/** Cancels a charge in progress (giving up, retreating, stopping). */
	void CancelCharge() const;

	/** Advances the strafe direction swap and applies the strafe. */
	void UpdateStrafe(AStartechEnemyShip& Ship, float DeltaSeconds);

	/** Where a circler out of range heads: the player, offset to the strafe side by up to PreferredRange (see FlankBlendStart). */
	FVector GetFlankPoint(const AStartechEnemyShip& Ship, const FVector& PlayerLocation) const;

	// Turns toward and flies at Destination. Returns true (and stops) once within ArrivalRadius.
	bool FlyToward(AStartechEnemyShip& Ship, const UEnemyData& Data, const FVector& Destination, float MaxThrottle) const;

	// Brings the ship to a stop in place.
	static void HoldPosition(AStartechEnemyShip& Ship);
	static void HoldFire(AStartechEnemyShip& Ship, float DeltaSeconds);

	float GetVerticalIntent(float HeightDifference) const;
	FVector PickWanderPoint(const UEnemyData& Data) const;
	float PickStrafeSwapTime() const;

	AStartechEnemyShip* GetShip() const;

	// The player pawn, or null if there is none or it is dead.
	const APawn* GetLivingPlayer() const;

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	/** Any damage anywhere: resets the quiet time when this enemy damaged the player. */
	void HandleAnyDamage(const UHealthComponent& Health, float ShieldDamage, float HullDamage, AActor* Instigator,
		bool bShieldDrain, bool bFatal);

	EBrainState State = EBrainState::Idle;
	FVector HomeLocation = FVector::ZeroVector;

	FVector WanderPoint = FVector::ZeroVector;
	bool bHasWanderPoint = false;

	float StrafeDirection = 1.f;
	float StrafeSwapTimeLeft = 0.f;

	/** Seconds engaged since the last damage that counts for the quiet leash. */
	float QuietTime = 0.f;

	/** Seconds the player has been beyond the far leash distance without a break. */
	float FarTime = 0.f;

	bool bRetreating = false;
	float ChargeWaitLeft = 0.f;

	bool bAfterburning = false;
	float AfterburnLeft = 0.f;
	float AfterburnCooldownLeft = 0.f;

	const TCHAR* LastGiveUpReason = TEXT("");

	bool bStopped = false;

	FDelegateHandle AnyDamageHandle;
};
