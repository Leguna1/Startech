#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Core/SectorDirectorSubsystem.h"
#include "EnemyData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gates/JumpGate.h"
#include "Gates/WarpTransitionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerProfileSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "MapMarkerComponent.h"
#include "StartechEnemyShip.h"
#include "StartechSettings.h"
#include "Tiers/StartechTierSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechTierConsole, Log, All);

/**
 * Tier debug commands (non-shipping):
 *   Startech.Test.Tier                      logs the current sector, its tier and values, the spawn tier, and every
 *                                           sector's tier
 *   Startech.Test.TierPoolCheck record      remembers every enemy in play with its tier and max hull and shield
 *   Startech.Test.TierPoolCheck check       for every remembered enemy back in play in a sector of another tier, checks
 *                                           that the same actor's max hull and shield follow the new tier; logs
 *                                           "[TierPool] PASS" or "[TierPool] FAIL"
 *   Startech.Test.Warp <SectorId>           warps the player ship as a gate would (fade, regenerate, arrival gate)
 *   Startech.Test.Where                     logs the sector, the ship's location, the nearest gate and where it leads,
 *                                           each gate marker's label and tier suffix, and the enemies by type
 */
namespace StartechTierConsole
{
	/** One tier's values on one line. */
	static FString Describe(const FSectorTierData& Data)
	{
		FString Minerals;
		for (const FTierMineralWeight& Weight : Data.MineralWeights)
		{
			Minerals += FString::Printf(TEXT(" %s=%g"), *Weight.Mineral.ToString(), Weight.Weight);
		}

		return FString::Printf(TEXT("%s hull x%g shield x%g damage x%g kill credits x%g loot x%g colour %s minerals:%s"),
			*Data.DisplayName.ToString(), Data.EnemyHullMultiplier, Data.EnemyShieldMultiplier,
			Data.EnemyDamageMultiplier, Data.KillCreditsMultiplier, Data.LootAmountMultiplier,
			*StaticEnum<ETierColorRole>()->GetNameStringByValue(static_cast<int64>(Data.DangerColor)),
			Minerals.IsEmpty() ? TEXT(" (table's own)") : *Minerals);
	}

	static void Dump(const TArray<FString>& Args, UWorld* const World)
	{
		const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(World);
		if (!Tiers)
		{
			UE_LOG(LogStartechTierConsole, Warning, TEXT("No tier subsystem in this world."));
			return;
		}

		const USectorDirectorSubsystem* const Director = World->GetSubsystem<USectorDirectorSubsystem>();
		const FName Sector = Director ? Director->GetCurrentSectorId() : NAME_None;

		UE_LOG(LogStartechTierConsole, Display, TEXT("[Tier] sector %s: tier %d, %s"), *Sector.ToString(),
			Tiers->GetCurrentTier(), *Describe(Tiers->GetCurrentTierData()));
		const UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(World);
		UE_LOG(LogStartechTierConsole, Display, TEXT("[Tier] spawn tier %d, highest tier visited %d"), Tiers->GetSpawnTier(),
			Profile ? Profile->GetHighestTierVisited() : 0);

		for (const FSectorTierAssignment& Entry : UStartechSettings::Get().SectorTiers)
		{
			UE_LOG(LogStartechTierConsole, Display, TEXT("[Tier]   %s = tier %d"), *Entry.SectorId.ToString(),
				Tiers->GetTierForSector(Entry.SectorId));
		}
	}

	/** One enemy as recorded by TierPoolCheck record. */
	struct FRecordedEnemy
	{
		/** The actor. */
		TWeakObjectPtr<AStartechEnemyShip> Enemy;
		/** Its tier then. */
		int32 Tier = 0;
		/** Its max hull then. */
		float MaxHull = 0.f;
		/** Its max shield then. */
		float MaxShield = 0.f;
	};

	/** The enemies remembered by the last record. */
	static TArray<FRecordedEnemy> Recorded;

	/** True when Enemy is alive and placed in the current sector, not stored in the pool. */
	static bool IsInPlay(const AStartechEnemyShip& Enemy)
	{
		const UHealthComponent* const Health = Enemy.FindComponentByClass<UHealthComponent>();
		return !Enemy.IsHidden() && Health && !Health->IsDead();
	}

	static void PoolCheck(const TArray<FString>& Args, UWorld* const World)
	{
		const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(World);
		if (!Tiers)
		{
			UE_LOG(LogStartechTierConsole, Warning, TEXT("[TierPool] No tier subsystem in this world."));
			return;
		}

		if (Args.Num() > 0 && Args[0] == TEXT("record"))
		{
			Recorded.Reset();
			for (TActorIterator<AStartechEnemyShip> It(World); It; ++It)
			{
				const UHealthComponent* const Health = It->FindComponentByClass<UHealthComponent>();
				if (Health && IsInPlay(**It))
				{
					Recorded.Add({ *It, It->GetLifeTier(), Health->GetMaxHull(), Health->GetMaxShield() });
				}
			}
			UE_LOG(LogStartechTierConsole, Display, TEXT("[TierPool] recorded %d enemies in tier %d."), Recorded.Num(),
				Tiers->GetCurrentTier());
			return;
		}

		int32 Reused = 0;
		int32 Failed = 0;
		for (const FRecordedEnemy& Entry : Recorded)
		{
			const AStartechEnemyShip* const Enemy = Entry.Enemy.Get();
			const UHealthComponent* const Health = Enemy ? Enemy->FindComponentByClass<UHealthComponent>() : nullptr;
			if (!Health || !IsInPlay(*Enemy) || Enemy->GetLifeTier() == Entry.Tier || !Enemy->GetEnemyData())
			{
				continue;
			}

			++Reused;
			const FSectorTierData& Tier = Tiers->GetTierData(Enemy->GetLifeTier());
			const float ExpectedHull = Enemy->GetEnemyData()->MaxHull * Tier.EnemyHullMultiplier;
			const float ExpectedShield = Enemy->GetEnemyData()->MaxShield * Tier.EnemyShieldMultiplier;
			const bool bOk = FMath::IsNearlyEqual(Health->GetMaxHull(), ExpectedHull, 0.01f)
				&& FMath::IsNearlyEqual(Health->GetMaxShield(), ExpectedShield, 0.01f)
				&& FMath::IsNearlyEqual(Health->GetHull(), ExpectedHull, 0.01f)
				&& !FMath::IsNearlyEqual(Health->GetMaxHull(), Entry.MaxHull, 0.01f);
			Failed += bOk ? 0 : 1;
			UE_LOG(LogStartechTierConsole, Display,
				TEXT("[TierPool] %s %s: tier %d -> %d, max hull %.0f -> %.0f (expected %.0f), max shield %.0f -> %.0f (expected %.0f), hull %.0f"),
				bOk ? TEXT("ok  ") : TEXT("BAD "), *Enemy->GetName(), Entry.Tier, Enemy->GetLifeTier(), Entry.MaxHull,
				Health->GetMaxHull(), ExpectedHull, Entry.MaxShield, Health->GetMaxShield(), ExpectedShield,
				Health->GetHull());
		}

		const bool bPass = Reused > 0 && Failed == 0;
		UE_LOG(LogStartechTierConsole, Display, TEXT("[TierPool] %s: %d of %d recorded enemies reused in another tier, %d wrong."),
			bPass ? TEXT("PASS") : TEXT("FAIL"), Reused, Recorded.Num(), Failed);
	}

	static void Warp(const TArray<FString>& Args, UWorld* const World)
	{
		APawn* const Ship = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		UWarpTransitionComponent* const WarpComponent = Ship ? Ship->FindComponentByClass<UWarpTransitionComponent>() : nullptr;
		if (!WarpComponent || Args.IsEmpty())
		{
			UE_LOG(LogStartechTierConsole, Warning, TEXT("[Where] Usage: Startech.Test.Warp <SectorId> (needs the player ship)."));
			return;
		}
		const bool bStarted = WarpComponent->BeginWarp(FName(*Args[0]));
		UE_LOG(LogStartechTierConsole, Display, TEXT("[Where] warp to %s %s."), *Args[0], bStarted ? TEXT("started") : TEXT("REFUSED"));
	}

	static void Where(const TArray<FString>& Args, UWorld* const World)
	{
		const APawn* const Ship = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		const USectorDirectorSubsystem* const Director = World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
		if (!Ship || !Director)
		{
			UE_LOG(LogStartechTierConsole, Warning, TEXT("[Where] No player ship or sector director."));
			return;
		}

		const FVector Location = Ship->GetActorLocation();
		const AJumpGate* Nearest = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		int32 Gates = 0;
		for (TActorIterator<AJumpGate> It(World); It; ++It)
		{
			if (It->IsHidden())
			{
				continue;
			}
			++Gates;
			const double Distance = FVector::Dist(Location, It->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = *It;
			}
		}

		// Every gate's screen marker text, as the nav layer draws it: label, then the suffix in its colour.
		for (TActorIterator<AJumpGate> It(World); It; ++It)
		{
			const UMapMarkerComponent* const Marker = It->IsHidden() ? nullptr : It->FindComponentByClass<UMapMarkerComponent>();
			if (Marker)
			{
				UE_LOG(LogStartechTierConsole, Display, TEXT("[Where] gate marker \"%s\" + \"%s\" in %s"), *Marker->Label.ToString(),
					*Marker->LabelSuffix.ToString(),
					*StaticEnum<ETierColorRole>()->GetNameStringByValue(static_cast<int64>(Marker->LabelSuffixColor)));
			}
		}

		// Enemies in play by type, e.g. "DA_Enemy_Drifter x6".
		TArray<FString> Types;
		TArray<int32> Counts;
		for (TActorIterator<AStartechEnemyShip> It(World); It; ++It)
		{
			if (!IsInPlay(**It) || !It->GetEnemyData())
			{
				continue;
			}
			const FString Type = It->GetEnemyData()->GetName();
			const int32 Index = Types.Find(Type);
			if (Index == INDEX_NONE)
			{
				Types.Add(Type);
				Counts.Add(1);
			}
			else
			{
				++Counts[Index];
			}
		}
		FString Enemies;
		for (int32 Index = 0; Index < Types.Num(); ++Index)
		{
			Enemies += FString::Printf(TEXT(" %s x%d"), *Types[Index], Counts[Index]);
		}

		UE_LOG(LogStartechTierConsole, Display, TEXT("[Where] sector %s (came from %s), ship at %s, %d gate(s), nearest leads to %s at %.0f m; enemies:%s"),
			*Director->GetCurrentSectorId().ToString(), *Director->GetPreviousSectorId().ToString(), *Location.ToCompactString(),
			Gates, Nearest ? *Nearest->GetDestinationSectorId().ToString() : TEXT("-"), Nearest ? NearestDistance / 100.0 : 0.0,
			Enemies.IsEmpty() ? TEXT(" none") : *Enemies);
	}

	static FAutoConsoleCommandWithWorldAndArgs WarpCommand(TEXT("Startech.Test.Warp"),
		TEXT("Startech.Test.Warp <SectorId>: warps the player ship as a gate would."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Warp));

	static FAutoConsoleCommandWithWorldAndArgs WhereCommand(TEXT("Startech.Test.Where"),
		TEXT("Logs the sector, the ship's location, and the nearest gate and where it leads."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Where));

	static FAutoConsoleCommandWithWorldAndArgs PoolCheckCommand(TEXT("Startech.Test.TierPoolCheck"),
		TEXT("Startech.Test.TierPoolCheck record|check: checks that pooled enemies reused in another tier's sector get that tier's max hull and shield."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PoolCheck));

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(TEXT("Startech.Test.Tier"),
		TEXT("Logs the current sector's tier and its values, the spawn tier, and every sector's tier."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Dump));
}

#endif
