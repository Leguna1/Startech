#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/PrimitiveComponent.h"
#include "Death/ShipDeathComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LootPickup.h"
#include "StartechEnemyShip.h"
#include "StartechShipPawn.h"
#include "TargetableComponent.h"
#include "TimerManager.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechDeathTest, Log, All);

namespace StartechDeathCommands
{
	/** Ships whose dying state is shown: the player, enemies within this distance of it, and every wreck. */
	constexpr float NearbyCm = 30000.f;

	const TCHAR* GetStateName(const UShipDeathComponent& Death)
	{
		return Death.IsAlive() ? TEXT("alive") : (Death.IsDying() ? TEXT("dying") : TEXT("exploded"));
	}

	const TCHAR* GetResponseName(const ECollisionResponse Response)
	{
		return Response == ECR_Block ? TEXT("block") : (Response == ECR_Overlap ? TEXT("overlap") : TEXT("ignore"));
	}

	void LogShip(const AStartechShipBase& Ship)
	{
		const UShipDeathComponent* const Death = Ship.GetShipDeath();
		const UPrimitiveComponent* const Root = Cast<UPrimitiveComponent>(Ship.GetRootComponent());
		const UTargetableComponent* const Targetable = Ship.FindComponentByClass<UTargetableComponent>();
		if (!Death || !Root)
		{
			return;
		}

		UE_LOG(LogStartechDeathTest, Warning,
			TEXT("[Death] %s: %s %.2f s, tumble %.0f deg/s, flash %.2f, speed %.0f cm/s, %s, shots %s, ships %s, overlaps %s, %s"),
			*Ship.GetName(), GetStateName(*Death), Death->GetElapsed(), Death->GetTumbleRate(), Death->GetFlash(),
			Ship.GetSpeed(), Ship.IsHidden() ? TEXT("hidden") : TEXT("visible"),
			GetResponseName(Root->GetCollisionResponseToChannel(ECC_Visibility)),
			GetResponseName(Root->GetCollisionResponseToChannel(ECC_Pawn)),
			Root->GetGenerateOverlapEvents() ? TEXT("on") : TEXT("off"),
			Targetable && Targetable->IsTargetable() ? TEXT("targetable") : TEXT("untargetable"));
	}

	void Status(const TArray<FString>& Args, UWorld* const World)
	{
		if (!World)
		{
			return;
		}

		const AActor* const Player = UGameplayStatics::GetPlayerPawn(World, 0);
		if (const AStartechShipBase* const PlayerShip = Cast<AStartechShipBase>(Player))
		{
			LogShip(*PlayerShip);
		}

		for (TActorIterator<AStartechEnemyShip> It(World); It; ++It)
		{
			const UShipDeathComponent* const Death = It->GetShipDeath();
			const bool bNearby = Player && FVector::Dist(It->GetActorLocation(), Player->GetActorLocation()) < NearbyCm;
			if ((Death && !Death->IsAlive()) || (bNearby && !It->IsHidden()))
			{
				LogShip(**It);
			}
		}

		int32 Pickups = 0;
		for (TActorIterator<ALootPickup> It(World); It; ++It)
		{
			Pickups += It->IsHidden() ? 0 : 1;
		}
		UE_LOG(LogStartechDeathTest, Warning, TEXT("[Death] loot pickups in play: %d"), Pickups);
	}

	/** Armed by DeathShot: screenshots still to take, and how long after a fatal hit. */
	int32 ShotsLeft = 0;
	float ShotDelay = 0.5f;
	FDelegateHandle FatalHandle;

	void HandleDamage(const UHealthComponent& Health, float, float, AActor*, bool, const bool bFatal)
	{
		AActor* const Owner = Health.GetOwner();
		UWorld* const World = Owner ? Owner->GetWorld() : nullptr;
		if (!bFatal || ShotsLeft <= 0 || !World)
		{
			return;
		}

		--ShotsLeft;
		const TWeakObjectPtr<AActor> WeakOwner = Owner;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakOwner]()
		{
			if (const AStartechShipBase* const Ship = Cast<AStartechShipBase>(WeakOwner.Get()))
			{
				LogShip(*Ship);
			}
			FScreenshotRequest::RequestScreenshot(false);
			UE_LOG(LogStartechDeathTest, Warning, TEXT("[Death] screenshot of %s requested"), *GetNameSafe(WeakOwner.Get()));
		}), FMath::Max(ShotDelay, 0.01f), false);

		if (ShotsLeft <= 0)
		{
			UHealthComponent::OnAnyDamageApplied.Remove(FatalHandle);
			FatalHandle.Reset();
		}
	}

	void DeathShot(const TArray<FString>& Args, UWorld* const World)
	{
		ShotDelay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.5f;
		ShotsLeft = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3;
		if (!FatalHandle.IsValid() && ShotsLeft > 0)
		{
			FatalHandle = UHealthComponent::OnAnyDamageApplied.AddStatic(&HandleDamage);
		}
		UE_LOG(LogStartechDeathTest, Warning, TEXT("[Death] next %d deaths: screenshot %.2f s after hull 0"),
			ShotsLeft, ShotDelay);
	}

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("Startech.Test.Death"),
		TEXT("Logs [Death] lines: the dying state, tumble, flash, collision and targetability of the player, nearby enemies and every wreck, and the loot pickups in play."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));

	FAutoConsoleCommandWithWorldAndArgs ShotCommand(TEXT("Startech.Test.DeathShot"),
		TEXT("Startech.Test.DeathShot [Seconds=0.5] [Count=3]: takes a screenshot (and logs the ship's [Death] line) that long after each of the next Count ship deaths."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DeathShot));
}

#endif
