#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatTelemetrySubsystem.generated.h"

class AChargedShotProjectile;
class AActor;
class AStartechEnemyShip;
class UAmmoData;
class UHealthComponent;
class ULaserMountComponent;

/** How an engagement ended. */
enum class ECombatEngagementEnd : uint8
{
	/** Still running. */
	Open,
	/** The enemy died. */
	Killed,
	/** 15 s with no shot at it and no damage from it, or the session ended. */
	Disengaged,
	/** The player died. */
	PlayerDied
};

/** Rounds of one ammo fired in an engagement. */
struct STARTECH_API FCombatAmmoUse
{
	TWeakObjectPtr<const UAmmoData> Ammo;
	FString AmmoName;
	int32 Rounds = 0;
	double Cost = 0.0;
};

/**
 * One fight with one enemy: from the first player shot aimed at it, or the first damage it dealt the player, until it
 * dies, the player dies, or EngagementTimeout passes with neither.
 */
struct STARTECH_API FCombatEngagement
{
	TWeakObjectPtr<const AStartechEnemyShip> Enemy;

	/** The type's tag leaf ("Chaser") and the tier it spawned in. */
	FString EnemyType;
	int32 Tier = 1;

	/** The player's Lasers level when it started. */
	int32 LasersLevel = 0;

	/** The player's max hull + max shield when it started. */
	float PlayerMaxHealth = 0.f;

	/** World times (seconds). */
	double StartTime = 0.0;
	double LastActivityTime = 0.0;
	double EndTime = 0.0;

	/** The last player shot aimed at it, for TimeOnTarget; negative before the first. */
	double LastPlayerShotTime = -1.0;

	/** Seconds the player spent firing at it: each volley counts one shot interval, overlapping volleys once. */
	double TimeOnTarget = 0.0;

	/** Player shots aimed at it (all mounts), and those that damaged it. */
	int32 ShotsFired = 0;
	int32 ShotsHit = 0;

	/** Player shots at it whose trace hit another actor first (an asteroid), and those whose trace hit nothing. */
	int32 ShotsBlocked = 0;
	int32 ShotsMissed = 0;

	/** The first actor that blocked a shot, for the log. */
	FString FirstBlocker;

	/** Laser rounds by ammo type, with their cost (UCombatBalance::GetRoundCost). */
	TArray<FCombatAmmoUse> AmmoUsed;

	/** The enemy's shots, and those that damaged the player. */
	int32 EnemyShots = 0;
	int32 EnemyHits = 0;

	/** Damage the player took from it. */
	float ShieldTaken = 0.f;
	float HullTaken = 0.f;

	/** Its charged shots, those that hit the player, and their damage (part of the damage above). */
	int32 ChargedFired = 0;
	int32 ChargedHit = 0;
	float ChargedDamage = 0.f;

	/** The death drop: credits, item sell value (UCombatBalance::GetBundleValue) and ammo rounds. */
	int32 Credits = 0;
	double LootValue = 0.0;
	int32 LootAmmoRounds = 0;

	ECombatEngagementEnd End = ECombatEngagementEnd::Open;

	double GetAmmoCost() const;
	int32 GetRounds() const;

	/** The ammo with the most rounds, or "-". */
	FString GetMainAmmo() const;

	/** Gained (credits + loot value) minus the ammo spent. */
	double GetNetProfit() const { return LootValue - GetAmmoCost(); }
};

/**
 * The combat log (development builds only; not created in shipping). Records every engagement between the player and
 * an enemy from the native combat events (UHealthComponent::OnAnyDamageApplied, ULaserMountComponent::OnAnyShotFired,
 * AStartechEnemyShip::OnAnyEnemyDied), so nothing binds to pooled enemies. Each finished engagement is appended to
 * Saved/CombatLogs/Combat_<time>.csv; Startech.Combat.Report prints the session summary per enemy type and tier.
 */
UCLASS()
class STARTECH_API UCombatTelemetrySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of WorldContext's world, or null (shipping, or not a game world). */
	static UCombatTelemetrySubsystem* Get(const UObject* WorldContext);

	/** Game and PIE worlds of non-shipping builds only. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Closes engagements idle for EngagementTimeout. */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The session summary, one line per enemy type and tier, then the totals. */
	TArray<FString> BuildReport() const;

	/** Starts a new session: forgets open and finished engagements (the CSV keeps them). For measuring styles apart. */
	void ResetSession();

	/** Engagements finished this session, oldest first. */
	const TArray<FCombatEngagement>& GetFinished() const { return Finished; }

	/** The CSV this session writes, empty until the first engagement ends. */
	const FString& GetCsvPath() const { return CsvPath; }

	/** The enemy's type name: its first Enemy.* tag's last part ("Chaser"). */
	static FString GetEnemyType(const AStartechEnemyShip& Enemy);

	/** Seconds with neither a shot at the enemy nor damage from it that end an engagement. */
	static constexpr double EngagementTimeout = 15.0;

private:
	void HandleShot(const ULaserMountComponent& Mount, AActor* AimedAt, const UAmmoData* Ammo);
	void HandleDamage(const UHealthComponent& Health, float ShieldDamage, float HullDamage, AActor* Instigator,
		bool bShieldDrain, bool bFatal);
	void HandleEnemyDied(AStartechEnemyShip& Enemy, AActor* Killer);
	void HandleChargedShotFired(const AChargedShotProjectile& Projectile);
	void HandleChargedShotEnded(const AChargedShotProjectile& Projectile, AActor* HitActor, bool bDamaged);

	/** The player pawn of this world, or null. */
	AActor* GetPlayer() const;

	/** The open engagement with Enemy, starting one if bStart. */
	FCombatEngagement* FindEngagement(const AStartechEnemyShip& Enemy, bool bStart);

	/** Ends the engagement at Index with End and records it. */
	void Close(int32 Index, ECombatEngagementEnd End);

	/** Appends one finished engagement to the CSV, writing the header first. */
	void WriteCsv(const FCombatEngagement& Engagement);

	double Now() const;

	TArray<FCombatEngagement> Open;
	TArray<FCombatEngagement> Finished;

	/** Enemy shots at the player per type, inside an engagement or not: proof that each type fires. */
	TArray<TPair<FString, int32>> EnemyShotsByType;

	double SessionStart = 0.0;
	FString CsvPath;

	FDelegateHandle ShotHandle;
	FDelegateHandle DamageHandle;
	FDelegateHandle DeathHandle;
	FDelegateHandle ChargedFiredHandle;
	FDelegateHandle ChargedEndedHandle;
};
