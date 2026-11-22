#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Death/ShipDeathComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "HomeRespawnSubsystem.h"
#include "LootPickup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Population/ActorPoolSubsystem.h"
#include "StartechEnemyShip.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "TargetableComponent.h"

namespace StartechDeathTests
{
	constexpr TCHAR DrifterClassPath[] = TEXT("/Game/Blueprints/BP_Enemy_Drifter.BP_Enemy_Drifter_C");
	constexpr TCHAR PlayerClassPath[] = TEXT("/Game/Blueprints/BP_ShipPawn.BP_ShipPawn_C");
	constexpr float Step = 1.f / 60.f;

	/**
	 * A game world whose actors begin play, with its subsystems (pool, despawn, respawn) but no sector, ticked by hand. Actors spawned
	 * in it begin play at once. Destroyed when it goes out of scope.
	 */
	struct FDeathWorld
	{
		FDeathWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("StartechDeathTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			// Not World->BeginPlay: the subsystems' OnWorldBeginPlay would start the start flow, which generates the
			// home sector. Actors spawned from now on still begin play.
			World->SetBegunPlay(true);
		}

		~FDeathWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		void Tick(const float Seconds) const
		{
			for (float Done = 0.f; Done < Seconds; Done += Step)
			{
				World->Tick(LEVELTICK_All, Step);
			}
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
	};

	/** Hull to 0 at once, through the health component, like the killing shot. */
	void Kill(const AStartechShipBase& Ship)
	{
		Ship.GetHealthComponent()->ApplyDirectDamage(1.e7f, 1.e7f, Ship.GetActorLocation());
	}

	UClass* LoadShipClass(FAutomationTestBase& Test, const TCHAR* const Path)
	{
		UClass* const Class = LoadClass<AStartechShipBase>(nullptr, Path);
		Test.TestNotNull(FString::Printf(TEXT("%s loads"), Path), Class);
		return Class;
	}

	bool SameResponses(const FCollisionResponseContainer& A, const FCollisionResponseContainer& B)
	{
		for (int32 Channel = 0; Channel < ECC_MAX; ++Channel)
		{
			if (A.GetResponse(static_cast<ECollisionChannel>(Channel)) != B.GetResponse(static_cast<ECollisionChannel>(Channel)))
			{
				return false;
			}
		}
		return true;
	}

	float GetDuration()
	{
		return UStartechSettings::Get().Death.DeathDuration;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechDeathEnemyTest, "Startech.Death.EnemyCreditedAtHullZeroLootAfterDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechDeathEnemyTest::RunTest(const FString& Parameters)
{
	using namespace StartechDeathTests;

	UClass* const DrifterClass = LoadShipClass(*this, DrifterClassPath);
	if (!DrifterClass || !TestTrue(TEXT("the dying state lasts"), GetDuration() > 0.2f))
	{
		return false;
	}

	const FDeathWorld Fixture;
	UActorPoolSubsystem* const Pool = Fixture.World->GetSubsystem<UActorPoolSubsystem>();
	AStartechEnemyShip* const Enemy = Cast<AStartechEnemyShip>(Pool->Acquire(DrifterClass, FTransform(FVector(0.f, 0.f, 1000.f))));
	if (!TestNotNull(TEXT("a drifter from the pool"), Enemy))
	{
		return false;
	}
	const FVector StartLocation = Enemy->GetActorLocation();
	Fixture.Tick(0.1f);
	AddInfo(FString::Printf(TEXT("diag: init %d actorsInit %d begun %d actorBegun %d tickReg %d tickEnabled %d moved %s physScene %d collision %d mesh %s"),
		Fixture.World->bIsWorldInitialized, Fixture.World->AreActorsInitialized(), Fixture.World->HasBegunPlay(),
		Enemy->HasActorBegunPlay(), Enemy->PrimaryActorTick.IsTickFunctionRegistered(), Enemy->PrimaryActorTick.IsTickFunctionEnabled(),
		*(Enemy->GetActorLocation() - StartLocation).ToCompactString(), Fixture.World->GetPhysicsScene() != nullptr,
		static_cast<int32>(Cast<UPrimitiveComponent>(Enemy->GetRootComponent())->GetCollisionEnabled()),
		*GetNameSafe(Cast<UStaticMeshComponent>(Enemy->GetRootComponent())->GetStaticMesh())));

	UTargetableComponent* const Targetable = Enemy->FindComponentByClass<UTargetableComponent>();
	TestTrue(TEXT("alive: targetable"), Targetable && Targetable->IsTargetable());
	TestTrue(TEXT("alive: blocks shots"), Fixture.SweepHits(*Enemy, ECC_Visibility));
	TestTrue(TEXT("alive: blocks ships"), Fixture.SweepHits(*Enemy, ECC_Pawn));

	int32 DiedEvents = 0;
	const FDelegateHandle DiedHandle = AStartechEnemyShip::OnAnyEnemyDied.AddLambda(
		[&DiedEvents, Enemy](AStartechEnemyShip& Dead, AActor*) { DiedEvents += &Dead == Enemy ? 1 : 0; });
	int32 Secondaries = 0;
	Enemy->GetShipDeath()->OnSecondaryExplosion.AddLambda([&Secondaries](const FVector&) { ++Secondaries; });
	TOptional<FVector> ExplodedAt;
	Enemy->GetShipDeath()->OnExploded.AddLambda([&ExplodedAt](const FVector& Location) { ExplodedAt = Location; });

	Kill(*Enemy);
	AStartechEnemyShip::OnAnyEnemyDied.Remove(DiedHandle);

	// The kill counts at hull 0, in the same call, with the rewards already decided.
	TestEqual(TEXT("died event at hull 0"), DiedEvents, 1);
	TestFalse(TEXT("drop rolled at hull 0"), Enemy->GetLastDrop().IsEmpty());
	TestTrue(TEXT("dying"), Enemy->GetShipDeath()->IsDying());
	TestTrue(TEXT("dying: untargetable"), Targetable && !Targetable->IsTargetable());
	TestFalse(TEXT("dying: shots pass through"), Fixture.SweepHits(*Enemy, ECC_Visibility));
	TestFalse(TEXT("dying: ships pass through"), Fixture.SweepHits(*Enemy, ECC_Pawn));
	TestEqual(TEXT("no loot at hull 0"), Fixture.GetPickups().Num(), 0);

	Fixture.Tick(GetDuration() * 0.5f);
	AddInfo(FString::Printf(TEXT("diag2: time %.3f real %.3f paused %d elapsed %.3f deathTickReg %d deathTickEnabled %d collections %d"),
		Fixture.World->GetTimeSeconds(), Fixture.World->GetRealTimeSeconds(), Fixture.World->IsPaused(), Enemy->GetShipDeath()->GetElapsed(),
		Enemy->GetShipDeath()->PrimaryComponentTick.IsTickFunctionRegistered(), Enemy->GetShipDeath()->IsComponentTickEnabled(),
		Fixture.World->GetLevelCollections().Num()));
	TestTrue(TEXT("halfway: still dying"), Enemy->GetShipDeath()->IsDying());
	TestFalse(TEXT("halfway: visible"), Enemy->IsHidden());
	TestTrue(TEXT("halfway: tumbling"), Enemy->GetShipDeath()->GetTumbleRate() > 0.f);
	TestEqual(TEXT("halfway: still no loot"), Fixture.GetPickups().Num(), 0);
	TestFalse(TEXT("halfway: shots still pass through"), Fixture.SweepHits(*Enemy, ECC_Visibility));

	Fixture.Tick(GetDuration() * 0.5f + 0.1f);
	TestTrue(TEXT("exploded after the delay"), ExplodedAt.IsSet());
	TestTrue(TEXT("secondary explosions went off"), Secondaries >= UStartechSettings::Get().Death.MinSecondaryExplosions);

	const TArray<ALootPickup*> Pickups = Fixture.GetPickups();
	if (TestEqual(TEXT("one pickup after the delay"), Pickups.Num(), 1) && ExplodedAt.IsSet())
	{
		TestTrue(TEXT("pickup at the wreck's explosion"),
			Pickups[0]->GetActorLocation().Equals(ExplodedAt.GetValue(), 1.f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechDeathPoolResetTest, "Startech.Death.PooledEnemyComesBackClean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechDeathPoolResetTest::RunTest(const FString& Parameters)
{
	using namespace StartechDeathTests;

	UClass* const DrifterClass = LoadShipClass(*this, DrifterClassPath);
	if (!DrifterClass)
	{
		return false;
	}

	const FDeathWorld Fixture;
	UActorPoolSubsystem* const Pool = Fixture.World->GetSubsystem<UActorPoolSubsystem>();
	AStartechEnemyShip* const Enemy = Cast<AStartechEnemyShip>(Pool->Acquire(DrifterClass, FTransform(FVector(0.f, 0.f, 1000.f))));
	if (!TestNotNull(TEXT("a drifter from the pool"), Enemy))
	{
		return false;
	}
	Fixture.Tick(0.1f);

	UStaticMeshComponent* const Mesh = Cast<UStaticMeshComponent>(Enemy->GetRootComponent());
	UShipDeathComponent* const Death = Enemy->GetShipDeath();
	const TArray<TObjectPtr<UMaterialInterface>> AliveOverrides = Mesh->OverrideMaterials;
	const FCollisionResponseContainer AliveResponses = Mesh->GetCollisionResponseToChannels();
	const FRotator AliveRotation = Enemy->GetActorRotation();

	int32 Explosions = 0;
	Death->OnExploded.AddLambda([&Explosions](const FVector&) { ++Explosions; });

	// Released half-way through dying, as a sector switch does: tumbled, flashing, collision off.
	Kill(*Enemy);
	Fixture.Tick(GetDuration() * 0.5f);
	TestTrue(TEXT("flashing: dynamic materials on the mesh"), Mesh->OverrideMaterials.Num() > 0
		&& Mesh->OverrideMaterials[0] && Mesh->OverrideMaterials[0]->IsA<UMaterialInstanceDynamic>());
	TestFalse(TEXT("tumbled"), Enemy->GetActorRotation().Equals(AliveRotation, 1.f));
	Pool->Release(Enemy);

	AStartechEnemyShip* const Reused = Cast<AStartechEnemyShip>(Pool->Acquire(DrifterClass, FTransform(FVector(0.f, 0.f, 1000.f))));
	if (!TestTrue(TEXT("the pool hands the same enemy back"), Reused == Enemy))
	{
		return false;
	}

	UTargetableComponent* const Targetable = Enemy->FindComponentByClass<UTargetableComponent>();
	TestTrue(TEXT("reused: alive"), Death->IsAlive());
	TestFalse(TEXT("reused: death component not ticking"), Death->IsComponentTickEnabled());
	TestEqual(TEXT("reused: no tumble"), Death->GetTumbleRate(), 0.f);
	TestFalse(TEXT("reused: health not dead"), Enemy->GetHealthComponent()->IsDead());
	TestTrue(TEXT("reused: targetable"), Targetable && Targetable->IsTargetable());
	TestTrue(TEXT("reused: drop cleared"), Enemy->GetLastDrop().IsEmpty());
	TestTrue(TEXT("reused: collision responses restored"), SameResponses(Mesh->GetCollisionResponseToChannels(), AliveResponses));
	TestTrue(TEXT("reused: overlaps on"), Mesh->GetGenerateOverlapEvents());
	TestTrue(TEXT("reused: materials restored"), Mesh->OverrideMaterials == AliveOverrides);
	TestFalse(TEXT("reused: visible"), Enemy->IsHidden());
	TestTrue(TEXT("reused: blocks shots"), Fixture.SweepHits(*Enemy, ECC_Visibility));

	// Nothing from the previous life goes off later.
	Fixture.Tick(GetDuration() + 0.2f);
	TestEqual(TEXT("no explosion from the released death"), Explosions, 0);
	TestEqual(TEXT("no loot from the released death"), Fixture.GetPickups().Num(), 0);
	TestTrue(TEXT("still alive"), Death->IsAlive() && !Enemy->IsHidden());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechDeathPlayerTest, "Startech.Death.PlayerFlowStartsAfterDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechDeathPlayerTest::RunTest(const FString& Parameters)
{
	using namespace StartechDeathTests;

	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!PlayerClass)
	{
		return false;
	}

	const FDeathWorld Fixture;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStartechShipPawn* const Ship = Fixture.World->SpawnActor<AStartechShipPawn>(PlayerClass, FVector(0.f, 0.f, 1000.f),
		FRotator::ZeroRotator, Spawn);
	UHomeRespawnSubsystem* const Respawn = Fixture.World->GetSubsystem<UHomeRespawnSubsystem>();
	if (!TestNotNull(TEXT("player ship"), Ship) || !TestNotNull(TEXT("respawn subsystem"), Respawn))
	{
		return false;
	}
	Fixture.Tick(0.1f);

	Kill(*Ship);
	TestTrue(TEXT("dead at hull 0"), Ship->GetHealthComponent()->IsDead());
	TestTrue(TEXT("dying"), Ship->GetShipDeath()->IsDying());
	TestFalse(TEXT("hull 0: respawn sequence not started"), Respawn->IsRespawning());
	TestFalse(TEXT("hull 0: wreck visible"), Ship->IsHidden());
	TestFalse(TEXT("hull 0: shots pass through"), Fixture.SweepHits(*Ship, ECC_Visibility));

	Fixture.Tick(GetDuration() * 0.5f);
	TestFalse(TEXT("halfway: respawn sequence not started"), Respawn->IsRespawning());

	Fixture.Tick(GetDuration() * 0.5f + 0.1f);
	TestTrue(TEXT("after the delay: respawn sequence started"), Respawn->IsRespawning());
	TestTrue(TEXT("after the delay: hidden"), Ship->IsHidden());
	return true;
}

#endif
