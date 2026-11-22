#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetCompilingManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HealthComponent.h"
#include "LootPickup.h"
#include "Misc/AutomationTest.h"
#include "StartechShipBase.h"
#include "StartechShipPawn.h"

/** Shared by the automation tests that need ships in a real world (death sequence, enemy behaviour). */
namespace StartechTestWorld
{
	constexpr TCHAR DrifterClassPath[] = TEXT("/Game/Blueprints/BP_Enemy_Drifter.BP_Enemy_Drifter_C");
	constexpr TCHAR ChaserClassPath[] = TEXT("/Game/Blueprints/BP_Enemy_Chaser.BP_Enemy_Chaser_C");
	constexpr TCHAR RangedClassPath[] = TEXT("/Game/Blueprints/BP_Enemy_Ranged.BP_Enemy_Ranged_C");
	constexpr TCHAR PlayerClassPath[] = TEXT("/Game/Blueprints/BP_ShipPawn.BP_ShipPawn_C");
	constexpr float Step = 1.f / 60.f;

	/**
	 * A game world whose actors begin play, with its subsystems (pool, despawn, respawn) but no sector, ticked by hand.
	 * Actors spawned in it begin play at once. Destroyed when it goes out of scope.
	 *
	 * It has a bare game instance and AGameModeBase, because pawns take damage only with an authority game mode.
 * Three more things a hand-ticked world needs: each step is a new engine frame (tick functions run once per frame), no
	 * World->BeginPlay (the start flow would generate a sector), and finished asset compilation before spawning
	 * (LoadShipClass), or the first ship has no collision body.
	 */
	struct FTestWorld
	{
		FTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("StartechTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);

			// A bare game instance (not initialised, so no game-instance subsystems) and the engine's base game mode:
			// pawns take damage only in a world with an authority game mode (APawn::ShouldTakeDamage).
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			World->SetGameInstance(GameInstance);
			World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
			World->SetGameMode(FURL());

			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
		}

		~FTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			GameInstance->RemoveFromRoot();
		}

		FTestWorld(const FTestWorld&) = delete;
		FTestWorld& operator=(const FTestWorld&) = delete;

		/** Ticks the world for Seconds in fixed steps. */
		void Tick(const float Seconds) const
		{
			for (float Done = 0.f; Done < Seconds; Done += Step)
			{
				++GFrameCounter;
				World->Tick(LEVELTICK_All, Step);
			}
		}

		/** Ticks until Condition is true or Seconds pass. Returns the seconds it took, or a negative value on timeout. */
		template <typename TCondition>
		float TickUntil(const float Seconds, TCondition&& Condition) const
		{
			for (float Done = 0.f; Done < Seconds; Done += Step)
			{
				if (Condition())
				{
					return Done;
				}
				++GFrameCounter;
				World->Tick(LEVELTICK_All, Step);
			}
			return Condition() ? Seconds : -1.f;
		}

		/** The player ship at Location facing along Rotation, possessed by a player controller (so GetPlayerPawn finds it). */
		AStartechShipPawn* SpawnPlayer(UClass* const PlayerClass, const FVector& Location,
			const FRotator& Rotation = FRotator::ZeroRotator) const
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStartechShipPawn* const Ship = World->SpawnActor<AStartechShipPawn>(PlayerClass, Location, Rotation, Spawn);
			APlayerController* const Controller = World->SpawnActor<APlayerController>();
			if (Ship && Controller)
			{
				Controller->Possess(Ship);
			}
			return Ship;
		}

		/** Visible loot pickups in play. */
		TArray<ALootPickup*> GetPickups() const
		{
			TArray<ALootPickup*> Pickups;
			for (TActorIterator<ALootPickup> It(World); It; ++It)
			{
				if (!It->IsHidden())
				{
					Pickups.Add(*It);
				}
			}
			return Pickups;
		}

		/** True if a 50 cm sphere swept along X through Target's location on Channel hits Target. */
		bool SweepHits(const AActor& Target, const ECollisionChannel Channel) const
		{
			const FVector Center = Target.GetActorLocation();
			TArray<FHitResult> Hits;
			World->SweepMultiByChannel(Hits, Center - FVector(20000.f, 0.f, 0.f), Center + FVector(20000.f, 0.f, 0.f),
				FQuat::Identity, Channel, FCollisionShape::MakeSphere(50.f));
			return Hits.ContainsByPredicate([&Target](const FHitResult& Hit) { return Hit.GetActor() == &Target; });
		}

		UWorld* World = nullptr;
		UGameInstance* GameInstance = nullptr;
	};

	/** Hull to 0 at once, through the health component, like the killing shot. */
	inline void Kill(const AStartechShipBase& Ship)
	{
		Ship.GetHealthComponent()->ApplyDirectDamage(1.e7f, 1.e7f, Ship.GetActorLocation());
	}

	/** Loads a ship Blueprint class and finishes asset compilation, so ships spawned from it have collision. */
	inline UClass* LoadShipClass(FAutomationTestBase& Test, const TCHAR* const Path)
	{
		UClass* const Class = LoadClass<AStartechShipBase>(nullptr, Path);
		Test.TestNotNull(FString::Printf(TEXT("%s loads"), Path), Class);
#if WITH_EDITOR
		FAssetCompilingManager::Get().FinishAllCompilation();
#endif
		return Class;
	}
}

#endif
