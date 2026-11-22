#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipHealthChanged, float, Hull, float, Shield);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShipDeath, AActor*, Killer);

/** Broadcast when one health value (shield or hull) or its maximum changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShipValueChanged, float, Current, float, Max);

/**
 * Broadcast for damage actually taken by one layer. Amount is summed over the report interval for continuous
 * damage. SourceLocation is where the damage came from, or the ship's own location when the source is unknown.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnShipDamageTaken, float, Amount, bool, bHitShield, FVector, SourceLocation);

/** Broadcast when hull crosses the low-hull threshold, in either direction. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLowHullChanged, bool, bIsLow);

class UHealthComponent;

/**
 * Native, for every health component in every world: damage was just applied (values already changed, death not yet
 * broadcast). Instigator is the firing ship (null for damage with no source, e.g. leaving the sector); bShieldDrain for
 * the drainer; bFatal when this hit kills. The combat log listens.
 */
DECLARE_MULTICAST_DELEGATE_SixParams(FOnAnyHealthDamage, const UHealthComponent& /* Health */, float /* ShieldDamage */,
	float /* HullDamage */, AActor* /* Instigator */, bool /* bShieldDrain */, bool /* bFatal */);

// Shield and hull. Listens to the owner's damage events; never destroys the owner.
// Shared by the player and every enemy, so the HUD reads a locked target's health through it too.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Current hull. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHull() const { return Hull; }

	/** Current shield. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetShield() const { return Shield; }

	/** Hull when fully repaired. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHull() const { return MaxHull; }

	/** Shield when fully charged. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxShield() const { return MaxShield; }

	/** True once hull has reached zero, until Revive. */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	/** True while hull is below LowHullFraction of MaxHull. */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsLowHull() const { return bLowHull; }

	// Sets both maximums and fills hull and shield to them.
	void InitializeHealth(float InMaxHull, float InMaxShield);

	/**
	 * Changes both maximums without a refill, for upgrades. A raised maximum raises the current value by the same
	 * amount, so an upgrade never feels like losing health; a lowered one only clamps. Dead ships keep 0 hull.
	 */
	void SetMaximums(float InMaxHull, float InMaxShield);

	/** Shield points regained per second once regeneration has started. */
	float GetShieldRegenRate() const { return ShieldRegenRate; }

	/** Sets the shield regeneration per second (at least 0), for upgrades. */
	void SetShieldRegenRate(float InRate) { ShieldRegenRate = FMath::Max(InRate, 0.f); }

	// While set, all damage is ignored.
	void SetInvulnerable(const bool bInInvulnerable) { bInvulnerable = bInInvulnerable; }
	bool IsInvulnerable() const { return bInvulnerable; }

	// Adds hull up to MaxHull. Does nothing once dead.
	void RepairHull(float Amount);

	// Adds shield up to MaxShield. Does nothing once dead, and leaves the regen delay alone.
	void RestoreShield(float Amount);

	// Full hull and shield, and alive again.
	void Revive();

	/**
	 * Takes ShieldAmount from the shield and HullAmount from the hull directly, each clamped to what is left, as if
	 * hit from SourceLocation. Runs the same path as real damage (every event, death included), but skips the
	 * shield-first order so each layer can be tested on its own. For test commands. False if dead or invulnerable.
	 */
	bool ApplyDirectDamage(float ShieldAmount, float HullAmount, const FVector& SourceLocation);

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnShipHealthChanged OnHealthChanged;

	// Fired once, when hull reaches zero.
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnShipDeath OnDeath;

	/**
	 * Fires whenever the shield or MaxShield changes, including every frame of regeneration, and once at BeginPlay
	 * with the starting value. Never throttled.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnShipValueChanged OnShieldChanged;

	/**
	 * Fires whenever the hull or MaxHull changes, including every frame of docked repair, and once at BeginPlay
	 * with the starting value. Never throttled.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnShipValueChanged OnHullChanged;

	/**
	 * Fires for damage taken, once per layer hit: a hit that breaks through the shield fires once for the shield
	 * and once for the hull. Coalesced per damage source and layer: at most once every DamageReportInterval
	 * seconds, with the amount summed since the last report. A first hit after a quiet spell fires at once.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnShipDamageTaken OnDamageTaken;

	/** Fires when hull crosses LowHullFraction of MaxHull, in either direction. Not on every hit. */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnLowHullChanged OnLowHullChanged;

	/** See FOnAnyHealthDamage. */
	static FOnAnyHealthDamage OnAnyDamageApplied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0"))
	float MaxHull = 100.f;

	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0"))
	float MaxShield = 50.f;

	// Seconds without taking any damage before the shield starts to recover.
	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0"))
	float ShieldRegenDelay = 5.f;

	// Shield points per second.
	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0"))
	float ShieldRegenRate = 10.f;

	/** Hull below this fraction of MaxHull counts as low (OnLowHullChanged). */
	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0", ClampMax = "1"))
	float LowHullFraction = 0.25f;

	/** Shortest time between two OnDamageTaken reports for the same damage source and layer, in seconds. */
	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "0", Units = "s"))
	float DamageReportInterval = 0.2f;

private:
	/** Hull, shield and their maximums at one moment, to compare against after a change. */
	struct FHealthSnapshot
	{
		/** Hull at the moment taken. */
		float Hull = 0.f;
		/** Shield at the moment taken. */
		float Shield = 0.f;
		/** MaxHull at the moment taken. */
		float MaxHull = 0.f;
		/** MaxShield at the moment taken. */
		float MaxShield = 0.f;
	};

	/** Damage from one source to one layer, waiting to be reported through OnDamageTaken. */
	struct FDamageReport
	{
		/** The actor that caused the damage. Null for an unknown source. */
		TWeakObjectPtr<const AActor> Source;
		/** Which layer this report is for. */
		bool bHitShield = false;
		/** Damage taken since the last report. */
		float PendingAmount = 0.f;
		/** Where the latest damage came from. */
		FVector SourceLocation = FVector::ZeroVector;
		/** World time of the last report; very negative before the first. */
		double LastReportTime = -UE_BIG_NUMBER;
	};

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	/** Removes ShieldDamage from the shield and HullDamage from the hull, then raises every event, death included. */
	void ApplySplitDamage(float ShieldDamage, float HullDamage, const AActor* Source, const FVector& SourceLocation,
		AActor* Killer, bool bShieldDrain = false);

	/** The current values, for NotifyValuesChanged. */
	FHealthSnapshot TakeSnapshot() const;

	/**
	 * Broadcasts OnHealthChanged, then OnShieldChanged and OnHullChanged for whichever changed since Before, then
	 * updates the low-hull state. bContinuous marks per-frame changes (regen, repair), logged at Verbose only.
	 */
	void NotifyValuesChanged(const FHealthSnapshot& Before, bool bContinuous);

	/** Recomputes bLowHull and fires OnLowHullChanged if it changed. */
	void UpdateLowHull();

	/** Adds Amount to the report for Source and the layer, reporting at once if its interval has passed. */
	void ReportDamage(float Amount, bool bHitShield, const AActor* Source, const FVector& SourceLocation);

	/** Broadcasts Report's pending damage and starts its interval at Now. */
	void BroadcastReport(FDamageReport& Report, double Now);

	/** Timer callback: reports every pending amount whose interval has passed, and reschedules for the rest. */
	void FlushDueDamageReports();

	/** Reports every pending amount now, whatever its interval. Used at death so the last hits are not lost. */
	void FlushAllDamageReports();

	/** Starts the flush timer for the earliest pending report, unless it is already running. */
	void ScheduleDamageFlush();

	/** World time, or 0 without a world. */
	double GetNow() const;

	float Hull = 0.f;
	float Shield = 0.f;
	float TimeSinceDamage = 0.f;
	bool bDead = false;
	bool bInvulnerable = false;

	/** Last value broadcast through OnLowHullChanged. */
	bool bLowHull = false;

	/** One entry per damage source and layer seen recently. */
	TArray<FDamageReport> DamageReports;

	/** Pending flush of coalesced damage reports. */
	FTimerHandle DamageFlushTimer;
};
