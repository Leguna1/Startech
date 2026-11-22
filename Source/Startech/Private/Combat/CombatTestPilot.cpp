#include "Combat/CombatTestPilot.h"

#include "AmmoData.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/CombatTelemetrySubsystem.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "StartechEnemyShip.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "TargetableComponent.h"
#include "TargetingComponent.h"
#include "WeaponSystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechPilot, Log, All);

namespace CombatTestPilot
{
	/** Beyond this the pilot flies to the enemy instead of locking it (inside the targeting's 200 m lock range). */
	constexpr float LockDistance = 18000.f;

	/** Half-width of the band around FightDistance in which the throttle is zero. */
	constexpr float DistanceBand = 1000.f;

	/** How far outside the fight distance a jump lands. */
	constexpr float JumpMargin = 3000.f;
}

bool UCombatTestPilot::ShouldCreateSubsystem(UObject* const Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
#endif
}

TStatId UCombatTestPilot::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatTestPilot, STATGROUP_Tickables);
}

void UCombatTestPilot::Deinitialize()
{
	AStartechEnemyShip::OnAnyEnemyDied.Remove(DeathHandle);
	Super::Deinitialize();
}

void UCombatTestPilot::Start(const FString& Type, const float Seconds, const float DistanceCm)
{
	WantedType = Type;
	Remaining = Seconds;
	FightDistance = DistanceCm;
	Kills = 0;
	Target.Reset();
	Pilot = Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	bActive = Pilot.IsValid();
	if (!DeathHandle.IsValid())
	{
		DeathHandle = AStartechEnemyShip::OnAnyEnemyDied.AddUObject(this, &UCombatTestPilot::HandleEnemyDied);
	}
	UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] %s: fighting %s for %.0f s at %.0f m."),
		bActive ? TEXT("started") : TEXT("no player ship"), *Type, Seconds, DistanceCm / 100.f);
}

void UCombatTestPilot::Stop(const TCHAR* const Reason)
{
	if (AStartechShipPawn* const Ship = Pilot.Get())
	{
		Ship->SetFireHeld(false);
		Ship->SetThrottleInput(0.f);
		if (UTargetingComponent* const Targeting = Ship->GetTargeting())
		{
			Targeting->ClearTarget();
		}
	}
	if (bActive)
	{
		UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] stopped (%s) after %d kill(s)."), Reason, Kills);
	}
	bActive = false;
	Target.Reset();
}

AStartechEnemyShip* UCombatTestPilot::FindTarget(const AActor& Ship) const
{
	AStartechEnemyShip* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<AStartechEnemyShip> It(GetWorld()); It; ++It)
	{
		AStartechEnemyShip* const Enemy = *It;
		const UTargetableComponent* const Targetable = Enemy->FindComponentByClass<UTargetableComponent>();
		if (!Targetable || !Targetable->IsTargetable() || Enemy->IsHidden())
		{
			continue;
		}
		if (WantedType != TEXT("Any") && UCombatTelemetrySubsystem::GetEnemyType(*Enemy) != WantedType)
		{
			continue;
		}
		const double Distance = FVector::DistSquared(Enemy->GetActorLocation(), Ship.GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Enemy;
		}
	}
	return Best;
}

void UCombatTestPilot::LogStatus(const AStartechShipPawn& Ship, const AStartechEnemyShip* const Enemy, const TCHAR* const Doing)
{
	StatusIn -= GetWorld()->GetDeltaSeconds();
	if (StatusIn > 0.f)
	{
		return;
	}
	StatusIn = 5.f;
	const UTargetingComponent* const Targeting = Ship.GetTargeting();
	const UHealthComponent* const Health = Ship.GetHealthComponent();
	// What a shot at the target would hit right now: the enemy, something else, or nothing.
	FString Probe = TEXT("-");
	if (Enemy && Targeting)
	{
		FHitResult Hit;
		const FVector Aim = Targeting->GetAimPoint();
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(PilotProbe), false, &Ship);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Ship.GetActorLocation(), Aim + (Aim - Ship.GetActorLocation()).GetSafeNormal() * 2000.f,
			ECC_Visibility, Params);
		// The same line against the enemy's mesh alone: tests its geometry, not the scene.
		FHitResult MeshHit;
		const UStaticMeshComponent* const Mesh = Enemy->FindComponentByClass<UStaticMeshComponent>();
		const bool bMeshHit = Mesh && const_cast<UStaticMeshComponent*>(Mesh)->LineTraceComponent(MeshHit, Ship.GetActorLocation(),
			Aim + (Aim - Ship.GetActorLocation()).GetSafeNormal() * 2000.f, FCollisionQueryParams(SCENE_QUERY_STAT(PilotMeshProbe), false));
		// The smallest sphere swept along that line that touches the mesh: the width of the gap at its centre.
		FString Sweeps;
		for (const float Radius : { 50.f, 100.f, 200.f })
		{
			FHitResult SweepHit;
			const bool bSwept = Mesh && const_cast<UStaticMeshComponent*>(Mesh)->SweepComponent(SweepHit, Ship.GetActorLocation(),
				Aim + (Aim - Ship.GetActorLocation()).GetSafeNormal() * 2000.f, FQuat::Identity, FCollisionShape::MakeSphere(Radius));
			Sweeps += FString::Printf(TEXT(" r%.0f:%s"), Radius, bSwept ? TEXT("hit") : TEXT("miss"));
		}
		Probe = FString::Printf(TEXT("%s, sweeps%s, mesh alone %s (%s, collision %d), aim %.0f m from origin"),
			bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("nothing"), *Sweeps, bMeshHit ? TEXT("hit") : TEXT("missed"),
			Mesh ? *Mesh->GetName() : TEXT("no mesh"), Mesh ? static_cast<int32>(Mesh->GetCollisionEnabled()) : -1,
			FVector::Dist(Aim, Enemy->GetActorLocation()) / 100.f);
	}
	UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] %s: %s at %.0f m, lock %d, hull %.0f shield %.0f, %.0f s left; probe hits %s"),
		Doing, Enemy ? *UCombatTelemetrySubsystem::GetEnemyType(*Enemy) : TEXT("none"),
		Enemy ? FVector::Dist(Ship.GetActorLocation(), Enemy->GetActorLocation()) / 100.f : 0.f,
		Targeting ? static_cast<int32>(Targeting->GetLockState()) : -1,
		Health ? Health->GetHull() : 0.f, Health ? Health->GetShield() : 0.f, Remaining, *Probe);
}

void UCombatTestPilot::HandleEnemyDied(AStartechEnemyShip& Enemy, AActor* const Killer)
{
	if (bActive && Killer && Killer == Pilot.Get())
	{
		++Kills;
		UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] killed %s (tier %d), %d so far."),
			*UCombatTelemetrySubsystem::GetEnemyType(Enemy), Enemy.GetLifeTier(), Kills);
	}
}

void UCombatTestPilot::Tick(const float DeltaTime)
{
	if (!bActive)
	{
		return;
	}
	AStartechShipPawn* const Ship = Pilot.Get();
	const UHealthComponent* const Health = Ship ? Ship->GetHealthComponent() : nullptr;
	if (!Ship || !Health || Health->IsDead())
	{
		Stop(TEXT("player died"));
		return;
	}
	Remaining -= DeltaTime;
	if (Remaining <= 0.f)
	{
		Stop(TEXT("time up"));
		return;
	}
	if (Ship->IsDocked() || Ship->IsHubOpen())
	{
		LogStatus(*Ship, nullptr, TEXT("docked"));
		return;
	}

	// Keep the target while it lives; otherwise the nearest of the wanted type.
	AStartechEnemyShip* Enemy = Target.Get();
	const UTargetableComponent* Targetable = Enemy ? Enemy->FindComponentByClass<UTargetableComponent>() : nullptr;
	if (!Targetable || !Targetable->IsTargetable())
	{
		Enemy = FindTarget(*Ship);
		Targetable = Enemy ? Enemy->FindComponentByClass<UTargetableComponent>() : nullptr;
		Target = Enemy;
	}
	UTargetingComponent* const Targeting = Ship->GetTargeting();
	if (!Enemy || !Targetable || !Targeting)
	{
		LogStatus(*Ship, nullptr, TEXT("no enemy"));
		Ship->SetFireHeld(false);
		Ship->SetThrottleInput(0.f);
		return;
	}

	const float Distance = FVector::Dist(Ship->GetActorLocation(), Enemy->GetActorLocation());
	if (Distance > CombatTestPilot::LockDistance)
	{
		// Too far to lock. Flying there through the asteroid field is not what a session measures: jump to just
		// outside the fight distance, facing it. The fight itself is flown and fired normally.
		Targeting->ClearTarget();
		const FVector Direction = (Enemy->GetActorLocation() - Ship->GetActorLocation()).GetSafeNormal();
		Ship->SetActorLocationAndRotation(Enemy->GetActorLocation() - Direction * (FightDistance + CombatTestPilot::JumpMargin),
			Direction.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		Ship->SetFireHeld(false);
		UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] jumped to %s from %.0f m away."),
			*UCombatTelemetrySubsystem::GetEnemyType(*Enemy), Distance / 100.f);
		return;
	}

	// Locked, the pawn's combat stance faces the enemy and the aim point is on it.
	if (Targeting->GetCurrentTarget() != Targetable || Targeting->GetLockState() != ETargetLockState::Hard)
	{
		Targeting->LockTarget(const_cast<UTargetableComponent*>(Targetable));
	}
	Ship->SetThrottleInput(Distance > FightDistance + CombatTestPilot::DistanceBand ? 1.f
		: Distance < FightDistance - CombatTestPilot::DistanceBand ? -1.f : 0.f);

	const UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem();
	const bool bFire = Weapons && Distance <= Weapons->GetUsableWeaponRange();
	Ship->SetFireHeld(bFire);
	LogStatus(*Ship, Enemy, bFire ? TEXT("firing") : TEXT("closing"));
}

#if !UE_BUILD_SHIPPING

namespace CombatTestPilotConsole
{
	/** Startech.Test.AutoFight <Type|Any|stop> [Seconds=120] [DistanceM=60] [AmmoTag] */
	static void AutoFight(const TArray<FString>& Args, UWorld* const World)
	{
		UCombatTestPilot* const Pilot = World ? World->GetSubsystem<UCombatTestPilot>() : nullptr;
		if (!Pilot)
		{
			UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] No pilot in this world (not a game world)."));
			return;
		}
		if (Args.IsEmpty() || Args[0].Equals(TEXT("stop"), ESearchCase::IgnoreCase))
		{
			Pilot->Stop(TEXT("command"));
			return;
		}

		// Optional ammo: select it first, as the action bar key would.
		if (Args.IsValidIndex(3))
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Args[3]), false);
			UAmmoData* const Ammo = UStartechSettings::Get().FindItemOfType<UAmmoData>(Tag);
			AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0));
			if (Ammo && Ship && Ship->GetWeaponSystem())
			{
				Ship->GetWeaponSystem()->SelectAmmo(Ammo);
				UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] selected %s."), *Ammo->DisplayName.ToString());
			}
			else
			{
				UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] unknown ammo %s; keeping the selection."), *Args[3]);
			}
		}

		const float Seconds = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 120.f;
		const float DistanceM = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 60.f;
		Pilot->Start(Args[0], Seconds, DistanceM * 100.f);
	}

	static FAutoConsoleCommandWithWorldAndArgs AutoFightCommand(TEXT("Startech.Test.AutoFight"),
		TEXT("Startech.Test.AutoFight <Type|Any|stop> [Seconds=120] [DistanceM=60] [AmmoTag]: flies the player into combat with the nearest enemy of Type (Drifter, Chaser, Ranged) until the time is up or the player dies."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AutoFight));
}

#endif
