#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Boost/ShipBoostComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LaserMountComponent.h"
#include "ShieldDrainMountComponent.h"
#include "StartechEnemyShip.h"
#include "TargetableComponent.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "StartechShipPawn.h"
#include "Upgrades/ShipUpgradeSubsystem.h"
#include "WeaponMountComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechUpgradesConsole, Log, All);

/**
 * Upgrade debug commands (non-shipping):
 *   Startech.Test.Upgrades                    logs "[Upgrades]": every track's level, next cost and CanUpgrade reason,
 *                                             then the player ship's resulting stats and the nearest living
 *                                             enemy's (upgrades must never reach enemies)
 *   Startech.Test.SetUpgrade <Track> <Level>  sets a track's level for free (anywhere, docked or not)
 *   Startech.Test.BuyUpgrade <Track>          buys the next level as the hub's Upgrade button does (docked, paid)
 */
namespace StartechUpgradesConsole
{
	static UShipUpgradeSubsystem* GetUpgrades(const UWorld* const World)
	{
		return UShipUpgradeSubsystem::Get(World);
	}

	static void LogStats(const UWorld* const World)
	{
		const AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		if (!Ship)
		{
			UE_LOG(LogStartechUpgradesConsole, Display, TEXT("[Upgrades] No player ship."));
			return;
		}

		const UHealthComponent* const Health = Ship->GetHealthComponent();
		const UInventoryComponent* const Inventory = Ship->GetInventory();
		const UShipBoostComponent* const Boost = Ship->GetBoost();
		UE_LOG(LogStartechUpgradesConsole, Display,
			TEXT("[Upgrades] stats: hull %.0f / %.0f shield %.0f / %.0f regen %.1f/s cargo %d / %d top speed %.0f (x%.2f) boost %.0f / %.0f regen %.1f/s, %d credits"),
			Health->GetHull(), Health->GetMaxHull(), Health->GetShield(), Health->GetMaxShield(), Health->GetShieldRegenRate(),
			Inventory->GetCargoUsed(), Inventory->GetCargoCapacity(), Ship->GetNormalMaxForwardSpeed(), Ship->GetNormalSpeedScale(),
			Boost->GetEnergy(), Boost->GetMaxEnergy(), Boost->GetRegenPerSecond(), Inventory->GetCredits());

		TInlineComponentArray<UWeaponMountComponent*> Mounts(Ship);
		for (const UWeaponMountComponent* const Mount : Mounts)
		{
			UE_LOG(LogStartechUpgradesConsole, Display, TEXT("[Upgrades] mount %s damage x%.2f"), *Mount->GetName(),
				Mount->GetDamageScale());
		}

		// The nearest targetable enemy: its stats come from its EnemyData and tier only.
		const AStartechEnemyShip* Nearest = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		for (TActorIterator<AStartechEnemyShip> It(const_cast<UWorld*>(World)); It; ++It)
		{
			const UTargetableComponent* const Targetable = It->FindComponentByClass<UTargetableComponent>();
			const double Distance = FVector::DistSquared(It->GetActorLocation(), Ship->GetActorLocation());
			if (Targetable && Targetable->IsTargetable() && Distance < NearestDistance)
			{
				Nearest = *It;
				NearestDistance = Distance;
			}
		}
		if (Nearest)
		{
			const UHealthComponent* const EnemyHealth = Nearest->GetHealthComponent();
			UE_LOG(LogStartechUpgradesConsole, Display,
				TEXT("[Upgrades] nearest enemy %s: max hull %.0f max shield %.0f top speed %.0f laser x%.2f drainer x%.2f"),
				*Nearest->GetClass()->GetName(), EnemyHealth->GetMaxHull(), EnemyHealth->GetMaxShield(),
				Nearest->GetNormalMaxForwardSpeed(), Nearest->GetLaserMountLeft()->GetDamageScale(),
				Nearest->GetShieldDrainMount() ? Nearest->GetShieldDrainMount()->GetDamageScale() : 0.f);
		}
	}

	static void Upgrades(const TArray<FString>& Args, UWorld* const World)
	{
		const UShipUpgradeSubsystem* const Subsystem = GetUpgrades(World);
		const UUpgradeCatalog* const Catalog = Subsystem ? Subsystem->GetCatalog() : nullptr;
		if (!Catalog)
		{
			UE_LOG(LogStartechUpgradesConsole, Warning, TEXT("[Upgrades] No upgrade subsystem or catalog."));
			return;
		}

		for (const FUpgradeTrack& Track : Catalog->Tracks)
		{
			const int32 Level = Subsystem->GetLevel(Track.TrackId);
			FUpgradeLevelCost Cost;
			FString CostText = TEXT("-");
			if (Subsystem->GetLevelCost(Track.TrackId, Level + 1, Cost))
			{
				CostText = FString::Printf(TEXT("%d cr"), Cost.Credits);
				for (const FUpgradeMineralCost& Mineral : Cost.Minerals)
				{
					CostText += FString::Printf(TEXT(" + %d %s"), Mineral.Count, *Mineral.MineralTag.ToString());
				}
			}
			FText Reason;
			const bool bCan = Subsystem->CanUpgrade(Track.TrackId, Reason);
			UE_LOG(LogStartechUpgradesConsole, Display, TEXT("[Upgrades] %s level %d / %d next %s: %s"),
				*Track.TrackId.ToString(), Level, Track.GetMaxLevel(), *CostText,
				bCan ? TEXT("can upgrade") : *Reason.ToString());
		}
		LogStats(World);
	}

	static void SetUpgrade(const TArray<FString>& Args, UWorld* const World)
	{
		UShipUpgradeSubsystem* const Subsystem = GetUpgrades(World);
		if (!Subsystem || Args.Num() < 2 || !Subsystem->SetLevel(FName(*Args[0]), FCString::Atoi(*Args[1])))
		{
			UE_LOG(LogStartechUpgradesConsole, Warning, TEXT("Usage: Startech.Test.SetUpgrade <Track> <Level>, with a track from the catalog."));
			return;
		}
		LogStats(World);
	}

	static void BuyUpgrade(const TArray<FString>& Args, UWorld* const World)
	{
		UShipUpgradeSubsystem* const Subsystem = GetUpgrades(World);
		if (!Subsystem || Args.Num() < 1)
		{
			UE_LOG(LogStartechUpgradesConsole, Warning, TEXT("Usage: Startech.Test.BuyUpgrade <Track>"));
			return;
		}
		const FName TrackId(*Args[0]);
		FText Reason;
		if (!Subsystem->CanUpgrade(TrackId, Reason))
		{
			UE_LOG(LogStartechUpgradesConsole, Display, TEXT("[Upgrades] buy %s refused: %s"), *TrackId.ToString(), *Reason.ToString());
			return;
		}
		Subsystem->Upgrade(TrackId);
		UE_LOG(LogStartechUpgradesConsole, Display, TEXT("[Upgrades] bought %s level %d"), *TrackId.ToString(),
			Subsystem->GetLevel(TrackId));
		LogStats(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs UpgradesCommand(TEXT("Startech.Test.Upgrades"),
		TEXT("Logs every upgrade track's level, next cost and reason, and the player ship's resulting stats."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Upgrades));

	static FAutoConsoleCommandWithWorldAndArgs SetUpgradeCommand(TEXT("Startech.Test.SetUpgrade"),
		TEXT("Startech.Test.SetUpgrade <Track> <Level>: sets an upgrade track's level for free."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetUpgrade));

	static FAutoConsoleCommandWithWorldAndArgs BuyUpgradeCommand(TEXT("Startech.Test.BuyUpgrade"),
		TEXT("Startech.Test.BuyUpgrade <Track>: buys the next level like the hub's Upgrade button."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BuyUpgrade));
}

#endif
