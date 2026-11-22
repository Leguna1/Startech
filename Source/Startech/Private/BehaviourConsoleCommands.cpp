#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Combat/ChargedShotSubsystem.h"
#include "Combat/EnemyChargeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EnemyBrainComponent.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "StartechEnemyShip.h"
#include "TimerManager.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechBehaviourTest, Log, All);

namespace StartechBehaviourCommands
{
	/** Enemies within this distance of the player are listed, plus every engaged or returning one. */
	constexpr float NearbyCm = 30000.f;

	void Enemies(const TArray<FString>& Args, UWorld* const World)
	{
		const AActor* const Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		if (!Player)
		{
			UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Brain] no player"));
			return;
		}

		int32 Listed = 0;
		for (TActorIterator<AStartechEnemyShip> It(World); It; ++It)
		{
			const UEnemyBrainComponent* const Brain = It->GetBrain();
			if (!Brain || It->IsHidden())
			{
				continue;
			}

			const float Distance = FVector::Dist(It->GetActorLocation(), Player->GetActorLocation());
			if (Distance > NearbyCm && !Brain->IsEngaged() && !Brain->IsReturningHome())
			{
				continue;
			}

			++Listed;
			FVector Origin;
			FVector Extent;
			It->GetActorBounds(true, Origin, Extent);
			FString Biggest;
			float BiggestSize = 0.f;
			TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
			for (const UPrimitiveComponent* const Primitive : Primitives)
			{
				if (Primitive && Primitive->IsRegistered() && Primitive->IsCollisionEnabled()
					&& Primitive->Bounds.BoxExtent.Size() > BiggestSize)
				{
					BiggestSize = Primitive->Bounds.BoxExtent.Size();
					Biggest = Primitive->GetName();
				}
			}
			UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Brain] %s bounds: extent %s, largest colliding %s (%.0f), offset %.0f"),
				*It->GetName(), *Extent.ToCompactString(), *Biggest, BiggestSize, (Origin - It->GetActorLocation()).Size());
			UE_LOG(LogStartechBehaviourTest, Warning,
				TEXT("[Brain] %s: %s%s, %.0f m, %.0f cm/s (top %.0f), afterburn %s, cooldown %.1f s"),
				*It->GetName(), Brain->GetStateName(),
				*FString(*Brain->GetLastGiveUpReason() ? FString::Printf(TEXT(" (gave up: %s)"), Brain->GetLastGiveUpReason()) : FString()),
				Distance / 100.f, It->GetSpeed(), It->GetMaxForwardSpeed() * It->GetBoostSpeedScale(),
				Brain->IsAfterburning() ? TEXT("on") : TEXT("off"), Brain->GetAfterburnCooldown());
		}
		UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Brain] %d enemy(ies) listed"), Listed);
	}

	/** Armed by ChargeShot: screenshots still to take, and how long into a telegraph. */
	int32 ShotsLeft = 0;
	float ShotDelay = 0.8f;
	FDelegateHandle ChargeHandle;

	void HandleChargeStarted(const UEnemyChargeComponent& Charge)
	{
		UWorld* const World = Charge.GetWorld();
		if (ShotsLeft <= 0 || !World)
		{
			return;
		}
		--ShotsLeft;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([]()
		{
			FScreenshotRequest::RequestScreenshot(true);
			UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Charge] screenshot requested"));
		}), FMath::Max(ShotDelay, 0.01f), false);
		if (ShotsLeft <= 0)
		{
			UChargedShotSubsystem::OnAnyChargeStarted.Remove(ChargeHandle);
			ChargeHandle.Reset();
		}
	}

	void ChargeShot(const TArray<FString>& Args, UWorld* const World)
	{
		ShotDelay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.8f;
		ShotsLeft = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 2;
		if (!ChargeHandle.IsValid() && ShotsLeft > 0)
		{
			ChargeHandle = UChargedShotSubsystem::OnAnyChargeStarted.AddStatic(&HandleChargeStarted);
		}
		UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Charge] next %d charge(s): screenshot (with the HUD) %.2f s into the telegraph"),
			ShotsLeft, ShotDelay);
	}

	FAutoConsoleCommandWithWorldAndArgs ChargeShotCommand(TEXT("Startech.Test.ChargeShot"),
		TEXT("Startech.Test.ChargeShot [Seconds=0.8] [Count=2]: a screenshot with the HUD that long into each of the next charged-shot telegraphs."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ChargeShot));

	void Heal(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipBase* const Player = World ? Cast<AStartechShipBase>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		UHealthComponent* const Health = Player ? Player->GetHealthComponent() : nullptr;
		if (!Health || Health->IsDead())
		{
			return;
		}
		Health->RepairHull(Health->GetMaxHull());
		Health->RestoreShield(Health->GetMaxShield());
		UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Brain] player healed to %.0f hull, %.0f shield"), Health->GetHull(), Health->GetShield());
	}

	FAutoConsoleCommandWithWorldAndArgs HealCommand(TEXT("Startech.Test.Heal"),
		TEXT("Startech.Test.Heal: refills the player's hull and shield (between measured fights; hull doesn't repair in flight)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Heal));

	void Turn(const TArray<FString>& Args, UWorld* const World)
	{
		APawn* const Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		if (!Player || Args.Num() < 1)
		{
			return;
		}
		Player->AddActorWorldRotation(FRotator(0.f, FCString::Atof(*Args[0]), 0.f));
		UE_LOG(LogStartechBehaviourTest, Warning, TEXT("[Brain] player turned to yaw %.0f"), Player->GetActorRotation().Yaw);
	}

	FAutoConsoleCommandWithWorldAndArgs TurnCommand(TEXT("Startech.Test.Turn"),
		TEXT("Startech.Test.Turn <Degrees>: yaws the player ship by that much at once (to put an enemy behind it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Turn));

	FAutoConsoleCommandWithWorldAndArgs EnemiesCommand(TEXT("Startech.Test.Enemies"),
		TEXT("Logs [Brain] lines for enemies near the player and every engaged or returning one: state, give-up reason, distance, speed, afterburner and cooldown."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Enemies));
}

#endif
