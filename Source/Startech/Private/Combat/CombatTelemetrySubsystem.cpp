#include "Combat/CombatTelemetrySubsystem.h"

#include "AmmoData.h"
#include "Combat/CombatBalance.h"
#include "Engine/World.h"
#include "EnemyData.h"
#include "HAL/FileManager.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LaserMountComponent.h"
#include "LaserWeaponData.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StartechEnemyShip.h"
#include "TargetableComponent.h"
#include "TargetingComponent.h"
#include "Upgrades/ShipUpgradeSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechCombatLog, Log, All);

namespace CombatTelemetry
{
	/** "Chaser" for an enemy whose data carries Enemy.Chaser. */
	FString EnemyType(const AStartechEnemyShip& Enemy)
	{
		const UEnemyData* const Data = Enemy.GetEnemyData();
		if (!Data || Data->EnemyTags.IsEmpty())
		{
			return Enemy.GetClass()->GetName();
		}
		FString Name = Data->EnemyTags.First().ToString();
		int32 Dot = INDEX_NONE;
		return Name.FindLastChar(TEXT('.'), Dot) ? Name.RightChop(Dot + 1) : Name;
	}

	const TCHAR* EndName(const ECombatEngagementEnd End)
	{
		switch (End)
		{
		case ECombatEngagementEnd::Killed: return TEXT("Killed");
		case ECombatEngagementEnd::Disengaged: return TEXT("Disengaged");
		case ECombatEngagementEnd::PlayerDied: return TEXT("PlayerDied");
		default: return TEXT("Open");
		}
	}
}

double FCombatEngagement::GetAmmoCost() const
{
	double Cost = 0.0;
	for (const FCombatAmmoUse& Use : AmmoUsed)
	{
		Cost += Use.Cost;
	}
	return Cost;
}

int32 FCombatEngagement::GetRounds() const
{
	int32 Rounds = 0;
	for (const FCombatAmmoUse& Use : AmmoUsed)
	{
		Rounds += Use.Rounds;
	}
	return Rounds;
}

FString FCombatEngagement::GetMainAmmo() const
{
	const FCombatAmmoUse* Main = nullptr;
	for (const FCombatAmmoUse& Use : AmmoUsed)
	{
		Main = !Main || Use.Rounds > Main->Rounds ? &Use : Main;
	}
	return Main ? Main->AmmoName : FString(TEXT("-"));
}

FString UCombatTelemetrySubsystem::GetEnemyType(const AStartechEnemyShip& Enemy)
{
	return CombatTelemetry::EnemyType(Enemy);
}

UCombatTelemetrySubsystem* UCombatTelemetrySubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCombatTelemetrySubsystem>() : nullptr;
}

bool UCombatTelemetrySubsystem::ShouldCreateSubsystem(UObject* const Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
#endif
}

void UCombatTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SessionStart = Now();
	ShotHandle = ULaserMountComponent::OnAnyShotFired.AddUObject(this, &UCombatTelemetrySubsystem::HandleShot);
	DamageHandle = UHealthComponent::OnAnyDamageApplied.AddUObject(this, &UCombatTelemetrySubsystem::HandleDamage);
	DeathHandle = AStartechEnemyShip::OnAnyEnemyDied.AddUObject(this, &UCombatTelemetrySubsystem::HandleEnemyDied);
}

void UCombatTelemetrySubsystem::Deinitialize()
{
	ULaserMountComponent::OnAnyShotFired.Remove(ShotHandle);
	UHealthComponent::OnAnyDamageApplied.Remove(DamageHandle);
	AStartechEnemyShip::OnAnyEnemyDied.Remove(DeathHandle);

	// The session is over: whatever is still running is written as disengaged.
	while (!Open.IsEmpty())
	{
		Close(Open.Num() - 1, ECombatEngagementEnd::Disengaged);
	}
	Super::Deinitialize();
}

TStatId UCombatTelemetrySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatTelemetrySubsystem, STATGROUP_Tickables);
}

double UCombatTelemetrySubsystem::Now() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

AActor* UCombatTelemetrySubsystem::GetPlayer() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void UCombatTelemetrySubsystem::Tick(const float DeltaTime)
{
	const double Time = Now();
	for (int32 Index = Open.Num() - 1; Index >= 0; --Index)
	{
		if (Time - Open[Index].LastActivityTime >= EngagementTimeout)
		{
			Close(Index, ECombatEngagementEnd::Disengaged);
		}
	}
}

FCombatEngagement* UCombatTelemetrySubsystem::FindEngagement(const AStartechEnemyShip& Enemy, const bool bStart)
{
	FCombatEngagement* const Found = Open.FindByPredicate([&Enemy](const FCombatEngagement& Each) { return Each.Enemy.Get() == &Enemy; });
	// A new engagement only with a living enemy (a dead one is still a valid actor until it despawns).
	const UTargetableComponent* const Targetable = Enemy.FindComponentByClass<UTargetableComponent>();
	if (Found || !bStart || !Targetable || !Targetable->IsTargetable())
	{
		return Found;
	}

	FCombatEngagement& Engagement = Open.AddDefaulted_GetRef();
	Engagement.Enemy = &Enemy;
	Engagement.EnemyType = CombatTelemetry::EnemyType(Enemy);
	Engagement.Tier = Enemy.GetLifeTier();
	Engagement.StartTime = Now();
	Engagement.LastActivityTime = Engagement.StartTime;

	const UCombatBalance* const Balance = UCombatBalance::Get();
	const UShipUpgradeSubsystem* const Upgrades = UShipUpgradeSubsystem::Get(this);
	Engagement.LasersLevel = Upgrades && Balance ? Upgrades->GetLevel(Balance->LasersTrackId) : 0;

	const AActor* const Player = GetPlayer();
	if (const UHealthComponent* const Health = Player ? Player->FindComponentByClass<UHealthComponent>() : nullptr)
	{
		Engagement.PlayerMaxHealth = Health->GetMaxHull() + Health->GetMaxShield();
	}
	return &Engagement;
}

void UCombatTelemetrySubsystem::HandleShot(const ULaserMountComponent& Mount, AActor* const AimedAt, const UAmmoData* const Ammo)
{
	if (Mount.GetWorld() != GetWorld())
	{
		return;
	}
	AActor* const Shooter = Mount.GetOwner();
	AActor* const Player = GetPlayer();
	if (!Shooter || !Player)
	{
		return;
	}

	// An enemy firing (enemies only ever fire at the player).
	if (const AStartechEnemyShip* const EnemyShooter = Cast<AStartechEnemyShip>(Shooter))
	{
		const FString Type = CombatTelemetry::EnemyType(*EnemyShooter);
		TPair<FString, int32>* const Tally = EnemyShotsByType.FindByPredicate([&Type](const TPair<FString, int32>& Each) { return Each.Key == Type; });
		if (Tally)
		{
			++Tally->Value;
		}
		else
		{
			EnemyShotsByType.Emplace(Type, 1);
		}
		if (FCombatEngagement* const Engagement = FindEngagement(*EnemyShooter, false))
		{
			++Engagement->EnemyShots;
		}
		return;
	}

	if (Shooter != Player)
	{
		return;
	}

	// A player shot: at the enemy its trace hit, else at the locked or soft target (a miss).
	AStartechEnemyShip* Target = Cast<AStartechEnemyShip>(AimedAt);
	const bool bTraceReachedTarget = Target != nullptr;
	if (!Target)
	{
		const UTargetingComponent* const Targeting = Player->FindComponentByClass<UTargetingComponent>();
		Target = Targeting ? Cast<AStartechEnemyShip>(Targeting->GetCurrentTargetActor()) : nullptr;
	}
	FCombatEngagement* const Engagement = Target ? FindEngagement(*Target, true) : nullptr;
	if (!Engagement)
	{
		return;
	}

	const double Time = Now();
	const ULaserWeaponData* const Weapon = Mount.GetWeapon();
	const double Interval = Weapon && Weapon->FireRate > 0.f ? 1.0 / Weapon->FireRate : 0.0;
	// The span of the volleys: the first shot starts it, each later volley adds up to one shot interval.
	Engagement->TimeOnTarget += Engagement->LastPlayerShotTime < 0.0 ? 0.0 : FMath::Min(Time - Engagement->LastPlayerShotTime, Interval);
	Engagement->LastPlayerShotTime = Time;
	Engagement->LastActivityTime = Time;
	++Engagement->ShotsFired;
	if (!bTraceReachedTarget)
	{
		++(AimedAt ? Engagement->ShotsBlocked : Engagement->ShotsMissed);
		if (AimedAt && Engagement->FirstBlocker.IsEmpty())
		{
			Engagement->FirstBlocker = AimedAt->GetName();
		}
	}

	FCombatAmmoUse* Use = Engagement->AmmoUsed.FindByPredicate([Ammo](const FCombatAmmoUse& Each) { return Each.Ammo.Get() == Ammo; });
	if (!Use)
	{
		Use = &Engagement->AmmoUsed.AddDefaulted_GetRef();
		Use->Ammo = Ammo;
		Use->AmmoName = Ammo ? Ammo->DisplayName.ToString() : FString(TEXT("?"));
	}
	++Use->Rounds;
	const UCombatBalance* const Balance = UCombatBalance::Get();
	Use->Cost += Balance ? Balance->GetRoundCost(Ammo) : 0.0;
}

void UCombatTelemetrySubsystem::HandleDamage(const UHealthComponent& Health, const float ShieldDamage, const float HullDamage,
	AActor* const Instigator, const bool bShieldDrain, const bool bFatal)
{
	if (Health.GetWorld() != GetWorld())
	{
		return;
	}
	const AActor* const Victim = Health.GetOwner();
	AActor* const Player = GetPlayer();
	if (!Victim || !Player)
	{
		return;
	}

	// The player hit an enemy with a laser. Only an open engagement counts it: the shot opened it, and a bolt still in
	// flight from a finished one must not start another.
	if (Instigator == Player && !bShieldDrain)
	{
		if (const AStartechEnemyShip* const Enemy = Cast<AStartechEnemyShip>(Victim))
		{
			if (FCombatEngagement* const Engagement = FindEngagement(*Enemy, false))
			{
				++Engagement->ShotsHit;
				Engagement->LastActivityTime = Now();
			}
		}
		return;
	}

	// An enemy hit the player.
	if (Victim == Player)
	{
		if (const AStartechEnemyShip* const Enemy = Cast<AStartechEnemyShip>(Instigator))
		{
			if (FCombatEngagement* const Engagement = FindEngagement(*Enemy, true))
			{
				++Engagement->EnemyHits;
				Engagement->ShieldTaken += ShieldDamage;
				Engagement->HullTaken += HullDamage;
				Engagement->LastActivityTime = Now();
			}
		}
		if (bFatal)
		{
			while (!Open.IsEmpty())
			{
				Close(Open.Num() - 1, ECombatEngagementEnd::PlayerDied);
			}
		}
	}
}

void UCombatTelemetrySubsystem::HandleEnemyDied(AStartechEnemyShip& Enemy, AActor* const Killer)
{
	if (Enemy.GetWorld() != GetWorld())
	{
		return;
	}
	const int32 Index = Open.IndexOfByPredicate([&Enemy](const FCombatEngagement& Each) { return Each.Enemy.Get() == &Enemy; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FCombatEngagement& Engagement = Open[Index];
	const FLootBundle& Drop = Enemy.GetLastDrop();
	const UCombatBalance* const Balance = UCombatBalance::Get();
	Engagement.Credits = Drop.Credits;
	Engagement.LootValue = Balance ? Balance->GetBundleValue(Drop) : Drop.Credits;
	for (const FAmmoStack& Stack : Drop.Ammo)
	{
		Engagement.LootAmmoRounds += FMath::RoundToInt32(Stack.Count);
	}
	// Killed by anything else (another enemy, the sector) it still ends; the player earned it only if they killed it.
	if (Killer != GetPlayer())
	{
		Engagement.Credits = 0;
		Engagement.LootValue = 0.0;
		Engagement.LootAmmoRounds = 0;
	}
	Close(Index, ECombatEngagementEnd::Killed);
}

void UCombatTelemetrySubsystem::Close(const int32 Index, const ECombatEngagementEnd End)
{
	FCombatEngagement Engagement = Open[Index];
	Open.RemoveAt(Index);
	Engagement.End = End;
	// The shot that opened an engagement by hitting the player was fired before it existed.
	Engagement.EnemyShots = FMath::Max(Engagement.EnemyShots, Engagement.EnemyHits);
	Engagement.EndTime = End == ECombatEngagementEnd::Disengaged ? Engagement.LastActivityTime : Now();

	UE_LOG(LogStartechCombatLog, Log, TEXT("[CombatLog] %s tier %d %s: on target %.2f s, total %.2f s, shots hit %d of %d (blocked %d%s, missed %d), rounds %d (%.1f cr), taken %.0f+%.0f, enemy hits %d of %d, gained %.1f"),
		*Engagement.EnemyType, Engagement.Tier, CombatTelemetry::EndName(End), Engagement.TimeOnTarget,
		Engagement.EndTime - Engagement.StartTime, Engagement.ShotsHit, Engagement.ShotsFired, Engagement.ShotsBlocked,
		Engagement.FirstBlocker.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" by %s"), *Engagement.FirstBlocker),
		Engagement.ShotsMissed, Engagement.GetRounds(),
		Engagement.GetAmmoCost(), Engagement.ShieldTaken, Engagement.HullTaken, Engagement.EnemyHits, Engagement.EnemyShots,
		Engagement.LootValue);

	WriteCsv(Engagement);
	Finished.Add(MoveTemp(Engagement));
}

void UCombatTelemetrySubsystem::WriteCsv(const FCombatEngagement& Engagement)
{
	const bool bFirst = CsvPath.IsEmpty();
	if (bFirst)
	{
		CsvPath = FPaths::ProjectSavedDir() / TEXT("CombatLogs") / FString::Printf(TEXT("Combat_%s.csv"),
			*FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S")));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(CsvPath), true);
	}

	FString Line;
	if (bFirst)
	{
		Line += TEXT("Start,Enemy,Tier,End,Ammo,LasersLevel,TimeOnTarget,TotalTime,ShotsFired,ShotsHit,ShotsBlocked,ShotsMissed,Rounds,RoundsByAmmo,AmmoCost,")
			TEXT("ShieldTaken,HullTaken,HealthLostPct,EnemyShots,EnemyHits,Credits,LootValue,LootAmmoRounds,NetProfit,PlayerDied\n");
	}

	FString ByAmmo;
	for (const FCombatAmmoUse& Use : Engagement.AmmoUsed)
	{
		ByAmmo += FString::Printf(TEXT("%s%s:%d"), ByAmmo.IsEmpty() ? TEXT("") : TEXT(";"), *Use.AmmoName, Use.Rounds);
	}
	const float Taken = Engagement.ShieldTaken + Engagement.HullTaken;
	Line += FString::Printf(TEXT("%.2f,%s,%d,%s,%s,%d,%.2f,%.2f,%d,%d,%d,%d,%d,%s,%.2f,%.1f,%.1f,%.1f,%d,%d,%d,%.1f,%d,%.1f,%d\n"),
		Engagement.StartTime, *Engagement.EnemyType, Engagement.Tier, CombatTelemetry::EndName(Engagement.End),
		*Engagement.GetMainAmmo(), Engagement.LasersLevel, Engagement.TimeOnTarget, Engagement.EndTime - Engagement.StartTime,
		Engagement.ShotsFired, Engagement.ShotsHit, Engagement.ShotsBlocked, Engagement.ShotsMissed, Engagement.GetRounds(),
		*ByAmmo, Engagement.GetAmmoCost(),
		Engagement.ShieldTaken, Engagement.HullTaken,
		Engagement.PlayerMaxHealth > 0.f ? 100.f * Taken / Engagement.PlayerMaxHealth : 0.f,
		Engagement.EnemyShots, Engagement.EnemyHits, Engagement.Credits, Engagement.LootValue, Engagement.LootAmmoRounds,
		Engagement.GetNetProfit(), Engagement.End == ECombatEngagementEnd::PlayerDied ? 1 : 0);

	FFileHelper::SaveStringToFile(Line, *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}

TArray<FString> UCombatTelemetrySubsystem::BuildReport() const
{
	struct FGroup
	{
		FString Type;
		int32 Tier = 0;
		int32 Engagements = 0;
		int32 Kills = 0;
		int32 PlayerDeaths = 0;
		double OnTarget = 0.0;
		double Total = 0.0;
		int32 ShotsFired = 0;
		int32 ShotsHit = 0;
		int32 EnemyShots = 0;
		int32 EnemyHits = 0;
		double Taken = 0.0;
		double LostShare = 0.0;
		double Gained = 0.0;
		double AmmoCost = 0.0;
		double CombatSeconds = 0.0;
	};

	TArray<FGroup> Groups;
	for (const FCombatEngagement& Each : Finished)
	{
		FGroup* Group = Groups.FindByPredicate([&Each](const FGroup& G) { return G.Type == Each.EnemyType && G.Tier == Each.Tier; });
		if (!Group)
		{
			Group = &Groups.AddDefaulted_GetRef();
			Group->Type = Each.EnemyType;
			Group->Tier = Each.Tier;
		}
		const double Taken = Each.ShieldTaken + Each.HullTaken;
		++Group->Engagements;
		Group->PlayerDeaths += Each.End == ECombatEngagementEnd::PlayerDied ? 1 : 0;
		Group->ShotsFired += Each.ShotsFired;
		Group->ShotsHit += Each.ShotsHit;
		Group->EnemyShots += Each.EnemyShots;
		Group->EnemyHits += Each.EnemyHits;
		Group->Gained += Each.LootValue;
		Group->AmmoCost += Each.GetAmmoCost();
		Group->CombatSeconds += Each.EndTime - Each.StartTime;
		if (Each.End == ECombatEngagementEnd::Killed)
		{
			// Averages per kill are over kills only.
			++Group->Kills;
			Group->OnTarget += Each.TimeOnTarget;
			Group->Total += Each.EndTime - Each.StartTime;
			Group->Taken += Taken;
			Group->LostShare += Each.PlayerMaxHealth > 0.f ? Taken / Each.PlayerMaxHealth : 0.0;
		}
	}
	Groups.Sort([](const FGroup& A, const FGroup& B) { return A.Tier != B.Tier ? A.Tier < B.Tier : A.Type < B.Type; });

	const double SessionMinutes = FMath::Max((Now() - SessionStart) / 60.0, 1e-6);
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Session %.1f min, %d engagement(s), CSV %s"), SessionMinutes, Finished.Num(),
		CsvPath.IsEmpty() ? TEXT("(none yet)") : *CsvPath));
	Lines.Add(TEXT("Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits"));
	double TotalGained = 0.0;
	double TotalCost = 0.0;
	for (const FGroup& G : Groups)
	{
		const double Kills = FMath::Max(G.Kills, 1);
		TotalGained += G.Gained;
		TotalCost += G.AmmoCost;
		Lines.Add(FString::Printf(TEXT("%-4d %-9s %3d %5d %4d  %5.2f/%6.2f s  %5.1f%%  %9.1f  %8.1f%%  %11.1f  %12.1f  %d/%d"),
			G.Tier, *G.Type, G.Engagements, G.Kills, G.PlayerDeaths, G.OnTarget / Kills, G.Total / Kills,
			G.ShotsFired > 0 ? 100.0 * G.ShotsHit / G.ShotsFired : 0.0, G.Taken / Kills, 100.0 * G.LostShare / Kills,
			(G.Gained - G.AmmoCost) / Kills, G.CombatSeconds > 0.0 ? 60.0 * (G.Gained - G.AmmoCost) / G.CombatSeconds : 0.0,
			G.EnemyShots, G.EnemyHits));
	}
	Lines.Add(FString::Printf(TEXT("Total gained %.0f cr, ammo %.0f cr, net %.0f cr: %.1f cr/min over the session."),
		TotalGained, TotalCost, TotalGained - TotalCost, (TotalGained - TotalCost) / SessionMinutes));

	FString Shots;
	for (const TPair<FString, int32>& Tally : EnemyShotsByType)
	{
		Shots += FString::Printf(TEXT("%s%s %d"), Shots.IsEmpty() ? TEXT("") : TEXT(", "), *Tally.Key, Tally.Value);
	}
	Lines.Add(TEXT("Enemy shots fired this session: ") + (Shots.IsEmpty() ? FString(TEXT("none")) : Shots));
	return Lines;
}
