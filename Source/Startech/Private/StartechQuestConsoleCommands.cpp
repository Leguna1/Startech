// Startech quest test commands (not in shipping builds):
//   Startech.Quest.KillNearestEnemy   destroys the living enemy nearest the player with no killer, so the kill must
//                                     not count for quests (the bridge logs "not the player, not reported").

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "StartechEnemyShip.h"
#include "StartechQuestBridge.h"
#include "TargetableComponent.h"

namespace StartechQuestCommands
{
	/** Startech.Quest.KillNearestEnemy */
	void KillNearestEnemy(const TArray<FString>& /*Args*/, UWorld* const World)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* const Player = PC ? PC->GetPawn() : nullptr;
		if (!Player)
		{
			UE_LOG(LogStartechQuest, Warning, TEXT("Startech.Quest.KillNearestEnemy: no player."));
			return;
		}

		AStartechEnemyShip* Nearest = nullptr;
		double NearestDistSq = TNumericLimits<double>::Max();
		for (TActorIterator<AStartechEnemyShip> It(World); It; ++It)
		{
			AStartechEnemyShip* const Enemy = *It;
			const UHealthComponent* const Health = Enemy->GetHealthComponent();
			const UTargetableComponent* const Targetable = Enemy->FindComponentByClass<UTargetableComponent>();

			// Pooled enemies stay valid while dead or parked; only a targetable, living one counts.
			if (!Health || Health->IsDead() || !Targetable || !Targetable->IsTargetable())
			{
				continue;
			}

			const double DistSq = FVector::DistSquared(Enemy->GetActorLocation(), Player->GetActorLocation());
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = Enemy;
			}
		}

		if (!Nearest)
		{
			UE_LOG(LogStartechQuest, Warning, TEXT("Startech.Quest.KillNearestEnemy: no living enemy."));
			return;
		}

		UHealthComponent* const Health = Nearest->GetHealthComponent();
		UE_LOG(LogStartechQuest, Display, TEXT("Startech.Quest.KillNearestEnemy: %s at %.0f m, no killer."),
			*Nearest->GetName(), FMath::Sqrt(NearestDistSq) / 100.0);

		// Direct damage has no instigator or causer, so the death has no killer.
		Health->ApplyDirectDamage(Health->GetShield(), Health->GetHull(), Nearest->GetActorLocation());
	}

	/** Registration. */
	FAutoConsoleCommandWithWorldAndArgs KillNearestEnemyCommand(TEXT("Startech.Quest.KillNearestEnemy"),
		TEXT("Destroys the living enemy nearest the player with no killer (must not count for quests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillNearestEnemy));
}

#endif
