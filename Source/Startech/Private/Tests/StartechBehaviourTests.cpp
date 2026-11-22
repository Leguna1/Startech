#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Boost/ShipBoostComponent.h"
#include "Combat/ChargedShotProjectile.h"
#include "Combat/ChargedShotSubsystem.h"
#include "Combat/EnemyChargeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "LaserMountComponent.h"
#include "EnemyBrainComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LaserWeaponData.h"
#include "Population/ActorPoolSubsystem.h"
#include "StartechEnemyShip.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Tests/StartechTestWorld.h"

namespace StartechBehaviourTests
{
	using namespace StartechTestWorld;

	/** An enemy of Class from the world's pool at Location, facing +X, as a sector spawns one. */
	AStartechEnemyShip* AcquireEnemy(const FTestWorld& Fixture, UClass* const Class, const FVector& Location)
	{
		UActorPoolSubsystem* const Pool = Fixture.World->GetSubsystem<UActorPoolSubsystem>();
		return Pool ? Cast<AStartechEnemyShip>(Pool->Acquire(Class, FTransform(Location))) : nullptr;
	}

	/** Counts the damage Enemy deals to the player while it lives. */
	struct FDamageCounter
	{
		explicit FDamageCounter(const AActor& InEnemy)
			: Enemy(&InEnemy)
		{
			Handle = UHealthComponent::OnAnyDamageApplied.AddLambda(
				[this](const UHealthComponent& Health, const float Shield, const float Hull, AActor* const Instigator, bool, bool)
				{
					if (Instigator == Enemy && Cast<AStartechShipPawn>(Health.GetOwner()))
					{
						Damage += Shield + Hull;
					}
				});
		}

		~FDamageCounter()
		{
			UHealthComponent::OnAnyDamageApplied.Remove(Handle);
		}

		FDamageCounter(const FDamageCounter&) = delete;
		FDamageCounter& operator=(const FDamageCounter&) = delete;

		const AActor* Enemy = nullptr;
		float Damage = 0.f;
		FDelegateHandle Handle;
	};

	float Distance(const AActor& A, const AActor& B)
	{
		return FVector::Dist(A.GetActorLocation(), B.GetActorLocation());
	}

	float GetLaserRange(const AStartechEnemyShip& Enemy)
	{
		const ULaserWeaponData* const Laser = Enemy.GetLaserWeapon();
		return Laser ? Laser->Range : 0.f;
	}

	/** Records one ranged enemy's charges: when each telegraph started and fired, and what each shot hit. */
	struct FChargeWatcher
	{
		FChargeWatcher(const FTestWorld& InFixture, AStartechEnemyShip& InEnemy)
			: World(InFixture.World), Enemy(&InEnemy)
		{
			UEnemyChargeComponent* const Charge = InEnemy.GetChargedShot();
			StartHandle = Charge->OnChargeStarted.AddLambda([this]() { Starts.Add(World->GetTimeSeconds()); });
			FireHandle = Charge->OnChargeFired.AddLambda([this](AChargedShotProjectile*) { Fires.Add(World->GetTimeSeconds()); });
			EndHandle = UChargedShotSubsystem::OnAnyChargedShotEnded.AddLambda(
				[this](const AChargedShotProjectile& Projectile, AActor* const HitActor, bool)
				{
					if (Projectile.GetLaunch().Shooter.Get() == Enemy)
					{
						Ended.Add(HitActor);
					}
				});
		}

		~FChargeWatcher()
		{
			if (IsValid(Enemy))
			{
				Enemy->GetChargedShot()->OnChargeStarted.Remove(StartHandle);
				Enemy->GetChargedShot()->OnChargeFired.Remove(FireHandle);
			}
			UChargedShotSubsystem::OnAnyChargedShotEnded.Remove(EndHandle);
		}

		FChargeWatcher(const FChargeWatcher&) = delete;
		FChargeWatcher& operator=(const FChargeWatcher&) = delete;

		UWorld* World = nullptr;
		AStartechEnemyShip* Enemy = nullptr;
		TArray<double> Starts;
		TArray<double> Fires;
		TArray<TWeakObjectPtr<AActor>> Ended;
		FDelegateHandle StartHandle;
		FDelegateHandle FireHandle;
		FDelegateHandle EndHandle;
	};

	/** A ranged enemy at the origin facing +X and the player PlayerOffset from it, invulnerable, with player rotation. */
	struct FRangedSetup
	{
		FRangedSetup(FAutomationTestBase& Test, const FTestWorld& Fixture, const FVector& PlayerOffset,
			const FRotator& PlayerRotation = FRotator(0.f, 180.f, 0.f))
		{
			UClass* const RangedClass = LoadShipClass(Test, RangedClassPath);
			UClass* const PlayerClass = LoadShipClass(Test, PlayerClassPath);
			if (!RangedClass || !PlayerClass)
			{
				return;
			}
			const FVector Origin(0.f, 0.f, 1000.f);
			Ranged = AcquireEnemy(Fixture, RangedClass, Origin);
			Player = Fixture.SpawnPlayer(PlayerClass, Origin + PlayerOffset, PlayerRotation);
			if (Player)
			{
				Player->GetHealthComponent()->SetInvulnerable(true);
			}
		}

		bool IsValid(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("ranged"), Ranged) && Test.TestNotNull(TEXT("player"), Player)
				&& Test.TestTrue(TEXT("ranged has a charged shot"), Ranged->GetBehaviour().bChargedShot);
		}

		AStartechEnemyShip* Ranged = nullptr;
		AStartechShipPawn* Player = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourDrifterPassiveTest, "Startech.Behaviour.DrifterNeverAttacksFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourDrifterPassiveTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	UClass* const DrifterClass = LoadShipClass(*this, DrifterClassPath);
	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!DrifterClass || !PlayerClass)
	{
		return false;
	}

	const FTestWorld Fixture;
	AStartechEnemyShip* const Drifter = AcquireEnemy(Fixture, DrifterClass, FVector(0.f, 0.f, 1000.f));
	// In front of it, inside its laser range and well inside the aggro range an aggressive type would use.
	AStartechShipPawn* const Player = Fixture.SpawnPlayer(PlayerClass, FVector(3000.f, 0.f, 1000.f), FRotator(0.f, 180.f, 0.f));
	if (!TestNotNull(TEXT("drifter"), Drifter) || !TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	TestTrue(TEXT("the player is the player pawn"), UGameplayStatics::GetPlayerPawn(Fixture.World, 0) == Player);

	FDamageCounter Dealt(*Drifter);
	Fixture.Tick(8.f);

	TestFalse(TEXT("never engages on sight"), Drifter->GetBrain()->IsEngaged());
	TestEqual(TEXT("never damages the player"), Dealt.Damage, 0.f);

	// The world does deliver its damage: one shot's worth from the drifter is counted.
	UGameplayStatics::ApplyDamage(Player, 1.f, nullptr, Drifter, UDamageType::StaticClass());
	TestTrue(TEXT("sanity: damage from the drifter would be counted"), Dealt.Damage > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourDrifterFightsBackTest, "Startech.Behaviour.DrifterFightsBackThenGivesUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourDrifterFightsBackTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	UClass* const DrifterClass = LoadShipClass(*this, DrifterClassPath);
	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!DrifterClass || !PlayerClass)
	{
		return false;
	}

	const FTestWorld Fixture;
	AStartechEnemyShip* const Drifter = AcquireEnemy(Fixture, DrifterClass, FVector(0.f, 0.f, 1000.f));
	AStartechShipPawn* const Player = Fixture.SpawnPlayer(PlayerClass, FVector(4000.f, 0.f, 1000.f), FRotator(0.f, 180.f, 0.f));
	if (!TestNotNull(TEXT("drifter"), Drifter) || !TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	Fixture.Tick(0.5f);

	FDamageCounter Dealt(*Drifter);
	const float Quiet = Drifter->GetBehaviour().LeashQuietSeconds;
	TestTrue(TEXT("drifter has a quiet leash"), Quiet > 5.f);

	// One hit from the player.
	UGameplayStatics::ApplyDamage(Drifter, 5.f, Player->GetController(), Player, UDamageType::StaticClass());
	TestTrue(TEXT("engages when hit by the player"), Drifter->GetBrain()->IsEngaged());

	Fixture.Tick(4.f);
	TestTrue(TEXT("fights back: damages the player"), Dealt.Damage > 0.f);

	// From here the player takes nothing (and so can't die); the drifter's quiet time counts only damage it takes.
	Player->GetHealthComponent()->SetInvulnerable(true);
	Fixture.Tick(Quiet - 4.f - 1.f);
	TestTrue(TEXT("still fighting just before the quiet leash"), Drifter->GetBrain()->IsEngaged());

	Fixture.Tick(1.5f);
	TestTrue(TEXT("gives up after the quiet leash"), Drifter->GetBrain()->IsReturningHome());
	TestEqual(TEXT("because it was quiet"), FString(Drifter->GetBrain()->GetLastGiveUpReason()), FString(TEXT("quiet")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourChaserCatchesTest, "Startech.Behaviour.ChaserAfterburnsAndCatchesAFleeingPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourChaserCatchesTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	UClass* const ChaserClass = LoadShipClass(*this, ChaserClassPath);
	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!ChaserClass || !PlayerClass)
	{
		return false;
	}

	const FTestWorld Fixture;
	AStartechEnemyShip* const Chaser = AcquireEnemy(Fixture, ChaserClass, FVector(0.f, 0.f, 1000.f));
	// 60 m ahead of it, both facing +X: inside its aggro range, beyond its afterburner trigger (1.2 × 40 m).
	AStartechShipPawn* const Player = Fixture.SpawnPlayer(PlayerClass, FVector(6000.f, 0.f, 1000.f));
	if (!TestNotNull(TEXT("chaser"), Chaser) || !TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	Player->GetHealthComponent()->SetInvulnerable(true);

	// Flies straight away at normal top speed (no boost).
	Player->SetThrottleInput(1.f);
	const float Range = GetLaserRange(*Chaser);

	const float BurnAt = Fixture.TickUntil(1.f, [Chaser]() { return Chaser->GetBrain()->IsAfterburning(); });
	TestTrue(TEXT("afterburns with the player out of range"), BurnAt >= 0.f);
	TestTrue(TEXT("afterburn is faster than the player's normal top speed"),
		Chaser->GetMaxForwardSpeed() * Chaser->GetBehaviour().AfterburnMultiplier > Player->GetNormalMaxForwardSpeed());

	const float CaughtAt = Fixture.TickUntil(8.f, [&]() { Player->SetThrottleInput(1.f); return Distance(*Chaser, *Player) <= Range; });
	AddInfo(FString::Printf(TEXT("caught (within %.0f m) after %.1f s; player at %.0f cm/s"), Range / 100.f, CaughtAt,
		Player->GetSpeed()));
	TestTrue(TEXT("catches a player flying away at normal top speed"), CaughtAt >= 0.f);
	TestTrue(TEXT("the player was really at top speed"), Player->GetSpeed() > 0.95f * Player->GetNormalMaxForwardSpeed());

	// The burn ends inside its range (at the latest when its 3 s run out), and the next waits for the cooldown.
	const float EndedAfter = Fixture.TickUntil(3.f, [&]() { Player->SetThrottleInput(1.f); return !Chaser->GetBrain()->IsAfterburning(); });
	TestTrue(TEXT("burn ends"), EndedAfter >= 0.f);
	TestTrue(TEXT("cooldown running"), Chaser->GetBrain()->GetAfterburnCooldown() > 0.f);
	TestEqual(TEXT("flight scale back to 1"), Chaser->GetBoostSpeedScale(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourChaserBoostEscapeTest, "Startech.Behaviour.BoostingAwayEscapesAChaser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourChaserBoostEscapeTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	UClass* const ChaserClass = LoadShipClass(*this, ChaserClassPath);
	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!ChaserClass || !PlayerClass)
	{
		return false;
	}

	const FTestWorld Fixture;
	AStartechEnemyShip* const Chaser = AcquireEnemy(Fixture, ChaserClass, FVector(0.f, 0.f, 1000.f));
	// Just outside its laser range.
	AStartechShipPawn* const Player = Fixture.SpawnPlayer(PlayerClass, FVector(5000.f, 0.f, 1000.f));
	if (!TestNotNull(TEXT("chaser"), Chaser) || !TestNotNull(TEXT("player"), Player) || !TestNotNull(TEXT("boost"), Player->GetBoost()))
	{
		return false;
	}
	Player->GetHealthComponent()->SetInvulnerable(true);
	TestTrue(TEXT("the boost is faster than the afterburner"),
		Player->GetNormalMaxForwardSpeed() * UStartechSettings::Get().Boost.MaxSpeedMultiplier
		> Chaser->GetMaxForwardSpeed() * Chaser->GetBehaviour().AfterburnMultiplier);

	// Flies away boosting until the energy runs out, then on at normal top speed.
	const float Range = GetLaserRange(*Chaser);
	float Closest = Distance(*Chaser, *Player);
	bool bBurned = false;
	Fixture.TickUntil(10.f, [&]()
	{
		Player->SetThrottleInput(1.f);
		Player->GetBoost()->SetBoostHeld(true);
		bBurned |= Chaser->GetBrain()->IsAfterburning();
		Closest = FMath::Min(Closest, Distance(*Chaser, *Player));
		return false;
	});

	const float Final = Distance(*Chaser, *Player);
	AddInfo(FString::Printf(TEXT("closest %.0f m, after 10 s %.0f m, chaser %s (%s)"), Closest / 100.f, Final / 100.f,
		Chaser->GetBrain()->GetStateName(), Chaser->GetBrain()->GetLastGiveUpReason()));
	TestTrue(TEXT("the chaser afterburned after it"), bBurned);
	TestTrue(TEXT("never caught (stayed out of its laser range)"), Closest > Range);
	TestTrue(TEXT("got well away"), Final > 3.f * Range);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourPoolCalmTest, "Startech.Behaviour.PooledEnemyComesBackCalm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourPoolCalmTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	UClass* const ChaserClass = LoadShipClass(*this, ChaserClassPath);
	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!ChaserClass || !PlayerClass)
	{
		return false;
	}

	const FTestWorld Fixture;
	AStartechEnemyShip* const Chaser = AcquireEnemy(Fixture, ChaserClass, FVector(0.f, 0.f, 1000.f));
	AStartechShipPawn* const Player = Fixture.SpawnPlayer(PlayerClass, FVector(6000.f, 0.f, 1000.f));
	if (!TestNotNull(TEXT("chaser"), Chaser) || !TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	Player->GetHealthComponent()->SetInvulnerable(true);

	// Engaged and mid-burn when the pool takes it back.
	const float BurnAt = Fixture.TickUntil(1.f, [Chaser]() { return Chaser->GetBrain()->IsAfterburning(); });
	if (!TestTrue(TEXT("afterburning before the release"), BurnAt >= 0.f))
	{
		return false;
	}
	TestTrue(TEXT("engaged before the release"), Chaser->GetBrain()->IsEngaged());

	// Far from the player this time, so nothing re-engages it on sight.
	UActorPoolSubsystem* const Pool = Fixture.World->GetSubsystem<UActorPoolSubsystem>();
	Pool->Release(Chaser);
	AStartechEnemyShip* const Reused = AcquireEnemy(Fixture, ChaserClass, FVector(0.f, 200000.f, 1000.f));
	if (!TestTrue(TEXT("the pool hands the same chaser back"), Reused == Chaser))
	{
		return false;
	}

	const UEnemyBrainComponent* const Brain = Chaser->GetBrain();
	TestFalse(TEXT("reused: not engaged"), Brain->IsEngaged());
	TestFalse(TEXT("reused: not returning"), Brain->IsReturningHome());
	TestFalse(TEXT("reused: not afterburning"), Brain->IsAfterburning());
	TestEqual(TEXT("reused: no cooldown"), Brain->GetAfterburnCooldown(), 0.f);
	TestEqual(TEXT("reused: flight scale 1"), Chaser->GetBoostSpeedScale(), 1.f);
	TestEqual(TEXT("reused: no speed"), Chaser->GetSpeed(), 0.f);

	Fixture.Tick(1.f);
	TestFalse(TEXT("reused: stays calm with the player out of aggro range"), Brain->IsEngaged() || Brain->IsAfterburning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourChaserDistanceTest, "Startech.Behaviour.ChaserNeverBumpsThePlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourChaserDistanceTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	UClass* const ChaserClass = LoadShipClass(*this, ChaserClassPath);
	UClass* const PlayerClass = LoadShipClass(*this, PlayerClassPath);
	if (!ChaserClass || !PlayerClass)
	{
		return false;
	}

	const FTestWorld Fixture;
	AStartechEnemyShip* const Chaser = AcquireEnemy(Fixture, ChaserClass, FVector(0.f, 0.f, 1000.f));
	// 75 m away and sitting still: the chaser burns in at full afterburner speed.
	AStartechShipPawn* const Player = Fixture.SpawnPlayer(PlayerClass, FVector(7500.f, 0.f, 1000.f));
	if (!TestNotNull(TEXT("chaser"), Chaser) || !TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	Player->GetHealthComponent()->SetInvulnerable(true);

	bool bBurned = false;
	float Closest = Distance(*Chaser, *Player);
	float TopSpeed = 0.f;
	Fixture.TickUntil(12.f, [&]()
	{
		bBurned |= Chaser->GetBrain()->IsAfterburning();
		TopSpeed = FMath::Max(TopSpeed, Chaser->GetSpeed());
		Closest = FMath::Min(Closest, Distance(*Chaser, *Player));
		return false;
	});

	// About two ship lengths (the chaser is 13.6 m long, the player 10.8 m).
	constexpr float MinimumGap = 2700.f;
	AddInfo(FString::Printf(TEXT("top speed %.0f cm/s, closest %.1f m, now %.1f m"), TopSpeed, Closest / 100.f,
		Distance(*Chaser, *Player) / 100.f));
	TestTrue(TEXT("burned in"), bBurned);
	TestTrue(TEXT("never closer than about two ship lengths"), Closest >= MinimumGap);
	TestTrue(TEXT("settles in its laser range"), Distance(*Chaser, *Player) <= GetLaserRange(*Chaser));
	TestTrue(TEXT("still engaged"), Chaser->GetBrain()->IsEngaged());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourTelegraphTest, "Startech.Behaviour.TelegraphComesBeforeTheShotAndAStillPlayerIsHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourTelegraphTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	const FRangedSetup Setup(*this, Fixture, FVector(9000.f, 0.f, 0.f));
	if (!Setup.IsValid(*this))
	{
		return false;
	}
	const FChargeWatcher Watch(Fixture, *Setup.Ranged);
	const float Telegraph = Setup.Ranged->GetBehaviour().ChargeTelegraphSeconds;

	// Two charges: the first 2-3 s in, the next 6-8 s after it.
	Fixture.TickUntil(16.f, [&Watch]() { return Watch.Ended.Num() >= 2; });

	AddInfo(FString::Printf(TEXT("%d charge(s), %d shot(s), %d ended"), Watch.Starts.Num(), Watch.Fires.Num(), Watch.Ended.Num()));
	if (!TestTrue(TEXT("two charged shots"), Watch.Fires.Num() >= 2 && Watch.Starts.Num() >= 2))
	{
		return false;
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const double Lead = Watch.Fires[Index] - Watch.Starts[Index];
		AddInfo(FString::Printf(TEXT("charge %d: telegraph %.3f s"), Index + 1, Lead));
		TestTrue(TEXT("the telegraph comes 1.2 s before the shot"), FMath::Abs(Lead - Telegraph) <= 2.0 * Step + 1e-3);
		TestTrue(TEXT("a player sitting still is hit"), Watch.Ended.IsValidIndex(Index) && Watch.Ended[Index].Get() == Setup.Player);
	}
	TestTrue(TEXT("charges 6-8 s apart"), Watch.Starts[1] - Watch.Starts[0] >= Setup.Ranged->GetBehaviour().ChargeIntervalMin - 0.05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourSteadyMoverTest, "Startech.Behaviour.ChargedShotHitsAPlayerKeepingItsCourse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourSteadyMoverTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	// Crossing the ranged enemy's line of fire at half speed the whole time: the shot leads it.
	const FRangedSetup Setup(*this, Fixture, FVector(8500.f, -2000.f, 0.f), FRotator(0.f, 90.f, 0.f));
	if (!Setup.IsValid(*this))
	{
		return false;
	}
	const FChargeWatcher Watch(Fixture, *Setup.Ranged);
	AStartechShipPawn* const Player = Setup.Player;

	Fixture.TickUntil(14.f, [&]() { Player->SetThrottleInput(0.5f); return Watch.Ended.Num() >= 1; });

	AddInfo(FString::Printf(TEXT("player speed %.0f cm/s, %d shot(s)"), Player->GetSpeed(), Watch.Fires.Num()));
	TestTrue(TEXT("the player kept moving"), Player->GetSpeed() > 1000.f);
	TestTrue(TEXT("a player keeping its course is hit"), Watch.Ended.Num() >= 1 && Watch.Ended[0].Get() == Player);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourDodgeTest, "Startech.Behaviour.ChargedShotMissesAPlayerWhoBoostsSideways",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourDodgeTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	// Still, nose across the ranged enemy's line of fire; it boosts that way once the telegraph starts.
	const FRangedSetup Setup(*this, Fixture, FVector(9000.f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f));
	if (!Setup.IsValid(*this) || !TestNotNull(TEXT("boost"), Setup.Player->GetBoost()))
	{
		return false;
	}
	const FChargeWatcher Watch(Fixture, *Setup.Ranged);
	AStartechShipPawn* const Player = Setup.Player;

	float SpeedAtShot = 0.f;
	Fixture.TickUntil(10.f, [&]()
	{
		const bool bDodging = Watch.Starts.Num() > 0;
		Player->SetThrottleInput(bDodging ? 1.f : 0.f);
		Player->GetBoost()->SetBoostHeld(bDodging);
		if (Watch.Fires.Num() > 0 && SpeedAtShot <= 0.f)
		{
			SpeedAtShot = Player->GetSpeed();
		}
		return Watch.Ended.Num() >= 1;
	});

	AddInfo(FString::Printf(TEXT("player at %.0f cm/s when the shot left"), SpeedAtShot));
	if (!TestTrue(TEXT("a charged shot was fired"), Watch.Ended.Num() >= 1))
	{
		return false;
	}
	TestTrue(TEXT("the player was boosting sideways"), SpeedAtShot > Player->GetNormalMaxForwardSpeed() * 0.5f);
	TestTrue(TEXT("a player boosting sideways during the telegraph is missed"), Watch.Ended[0].Get() != Player);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourAsteroidTest, "Startech.Behaviour.ChargedShotIsStoppedByAnAsteroid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourAsteroidTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	const FRangedSetup Setup(*this, Fixture, FVector(9000.f, 0.f, 0.f));
	UStaticMesh* const Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Setup.IsValid(*this) || !TestNotNull(TEXT("cube mesh"), Cube))
	{
		return false;
	}
	const FChargeWatcher Watch(Fixture, *Setup.Ranged);

	// Once the telegraph has started (so it had line of sight), a 30 m rock moves in halfway.
	AStaticMeshActor* Rock = nullptr;
	Fixture.TickUntil(10.f, [&]()
	{
		if (!Rock && Watch.Starts.Num() > 0)
		{
			const FVector Middle = (Setup.Ranged->GetActorLocation() + Setup.Player->GetActorLocation()) * 0.5;
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Rock = Fixture.World->SpawnActor<AStaticMeshActor>(Middle, FRotator::ZeroRotator, Spawn);
			Rock->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Rock->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Rock->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
			Rock->SetActorScale3D(FVector(30.f));
		}
		return Watch.Ended.Num() >= 1;
	});

	if (!TestTrue(TEXT("a charged shot was fired"), Rock && Watch.Ended.Num() >= 1))
	{
		return false;
	}
	TestTrue(TEXT("the rock stopped it"), Watch.Ended[0].Get() == Rock);
	const UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(Fixture.World);
	TestEqual(TEXT("nothing left flying"), Shots ? Shots->GetFlyingCount() : -1, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourDeathMidChargeTest, "Startech.Behaviour.RangedDyingMidChargeNeverFires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourDeathMidChargeTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	const FRangedSetup Setup(*this, Fixture, FVector(9000.f, 0.f, 0.f));
	if (!Setup.IsValid(*this))
	{
		return false;
	}
	const FChargeWatcher Watch(Fixture, *Setup.Ranged);
	UEnemyChargeComponent* const Charge = Setup.Ranged->GetChargedShot();

	const float StartedAt = Fixture.TickUntil(8.f, [Charge]() { return Charge->IsCharging(); });
	if (!TestTrue(TEXT("a charge started"), StartedAt >= 0.f))
	{
		return false;
	}
	Fixture.Tick(0.5f * Setup.Ranged->GetBehaviour().ChargeTelegraphSeconds);
	TestTrue(TEXT("still charging halfway"), Charge->IsCharging());

	Kill(*Setup.Ranged);
	TestFalse(TEXT("the charge stops at hull 0"), Charge->IsCharging());
	Fixture.Tick(3.f);

	TestEqual(TEXT("never fired"), Watch.Fires.Num(), 0);
	const UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(Fixture.World);
	TestTrue(TEXT("no warning left on the HUD's list"), Shots && Shots->GetActiveCharges().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourPooledRangedTest, "Startech.Behaviour.PooledRangedComesBackWithNoCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourPooledRangedTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	const FRangedSetup Setup(*this, Fixture, FVector(9000.f, 0.f, 0.f));
	if (!Setup.IsValid(*this))
	{
		return false;
	}
	AStartechEnemyShip* const Ranged = Setup.Ranged;
	UEnemyChargeComponent* const Charge = Ranged->GetChargedShot();
	const FChargeWatcher Watch(Fixture, *Ranged);

	UStaticMeshComponent* const Mesh = Cast<UStaticMeshComponent>(Ranged->GetRootComponent());
	const TArray<TObjectPtr<UMaterialInterface>> CalmOverrides = Mesh->OverrideMaterials;

	const float StartedAt = Fixture.TickUntil(8.f, [Charge]() { return Charge->IsCharging(); });
	if (!TestTrue(TEXT("a charge started"), StartedAt >= 0.f))
	{
		return false;
	}
	Fixture.Tick(0.5f);
	TestTrue(TEXT("glowing: dynamic materials on the hull"), Mesh->OverrideMaterials != CalmOverrides);

	// Released mid-charge, then reused far from the player.
	UActorPoolSubsystem* const Pool = Fixture.World->GetSubsystem<UActorPoolSubsystem>();
	Pool->Release(Ranged);
	UClass* const RangedClass = Ranged->GetClass();
	AStartechEnemyShip* const Reused = AcquireEnemy(Fixture, RangedClass, FVector(0.f, 200000.f, 1000.f));
	if (!TestTrue(TEXT("the pool hands the same enemy back"), Reused == Ranged))
	{
		return false;
	}

	TestFalse(TEXT("reused: not charging"), Charge->IsCharging());
	TestFalse(TEXT("reused: not ticking"), Charge->IsComponentTickEnabled());
	TestTrue(TEXT("reused: hull materials restored"), Mesh->OverrideMaterials == CalmOverrides);
	TestFalse(TEXT("reused: not engaged or retreating"), Ranged->GetBrain()->IsEngaged() || Ranged->GetBrain()->IsRetreating());
	const UChargedShotSubsystem* const Shots = UChargedShotSubsystem::Get(Fixture.World);
	TestTrue(TEXT("reused: no warning on the HUD's list"), Shots && Shots->GetActiveCharges().IsEmpty());

	Fixture.Tick(3.f);
	TestEqual(TEXT("the released charge never fired"), Watch.Fires.Num(), 0);
	TestFalse(TEXT("calm a while later"), Charge->IsCharging());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechBehaviourRetreatTest, "Startech.Behaviour.RangedRetreatsFromAPlayerTooClose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStartechBehaviourRetreatTest::RunTest(const FString& Parameters)
{
	using namespace StartechBehaviourTests;

	const FTestWorld Fixture;
	// 30 m away: inside 40% of its 90 m preferred range.
	const FRangedSetup Setup(*this, Fixture, FVector(3000.f, 0.f, 0.f));
	if (!Setup.IsValid(*this))
	{
		return false;
	}
	AStartechEnemyShip* const Ranged = Setup.Ranged;

	int32 ShotsWhileRetreating = 0;
	const FDelegateHandle ShotHandle = ULaserMountComponent::OnAnyShotFired.AddLambda(
		[&ShotsWhileRetreating, Ranged](const ULaserMountComponent& Mount, AActor*, const UAmmoData*)
		{
			if (Mount.GetOwner() == Ranged && Ranged->GetBrain()->IsRetreating())
			{
				++ShotsWhileRetreating;
			}
		});

	Fixture.Tick(0.2f);
	TestTrue(TEXT("engaged"), Ranged->GetBrain()->IsEngaged());
	TestTrue(TEXT("retreats from a player inside 40% of its range"), Ranged->GetBrain()->IsRetreating());

	const float BackAt = Fixture.TickUntil(15.f, [Ranged]() { return !Ranged->GetBrain()->IsRetreating(); });
	ULaserMountComponent::OnAnyShotFired.Remove(ShotHandle);
	const float BackDistance = Distance(*Ranged, *Setup.Player);
	AddInfo(FString::Printf(TEXT("back at %.0f m after %.1f s"), BackDistance / 100.f, BackAt));

	TestTrue(TEXT("stops retreating"), BackAt >= 0.f);
	TestTrue(TEXT("once back at its preferred range"), BackDistance >= Ranged->GetPreferredRange() - 100.f);
	TestEqual(TEXT("never fires while retreating"), ShotsWhileRetreating, 0);
	TestTrue(TEXT("still engaged afterwards"), Ranged->GetBrain()->IsEngaged());
	return true;
}

#endif
