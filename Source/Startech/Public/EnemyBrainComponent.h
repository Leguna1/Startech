#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyBrainComponent.generated.h"

class AController;
class APawn;
class AStartechEnemyShip;
class UDamageType;
class UEnemyData;

// Flies an AStartechEnemyShip using only the ship's public intent setters, facing functions
// and weapon system. Its type of behaviour comes from the ship's EnemyData.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UEnemyBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyBrainComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Stops all thinking, e.g. on death, until ResetBrain.
	void StopBrain();

	/**
	 * Restarts thinking from scratch, undoing StopBrain: idle, no wander point, ticking again, with Home as the
	 * point it guards and returns to. Called at the start of each life (a pooled enemy's new spawn location).
	 */
	void ResetBrain(const FVector& Home);

	/**
	 * The band around the preferred range in which the enemy holds still, as a fraction of it. An engaged enemy can
	 * sit as far out as PreferredRange × (1 + this), so its laser must reach that far (checked by the TTK table).
	 */
	float GetRangeTolerance() const { return RangeTolerance; }

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

	// Ranged enemies swap strafe direction after a random time in this range, in seconds.
	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float StrafeSwapMinTime = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Brain", meta = (ClampMin = "0"))
	float StrafeSwapMaxTime = 4.f;

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

	EBrainState State = EBrainState::Idle;
	FVector HomeLocation = FVector::ZeroVector;

	FVector WanderPoint = FVector::ZeroVector;
	bool bHasWanderPoint = false;

	float StrafeDirection = 1.f;
	float StrafeSwapTimeLeft = 0.f;

	bool bStopped = false;
};
