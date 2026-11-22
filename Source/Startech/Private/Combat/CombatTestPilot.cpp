#include "Combat/CombatTestPilot.h"

#include "AmmoData.h"
#include "Combat/ChargedShotSubsystem.h"
#include "Combat/EnemyChargeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/CombatTelemetrySubsystem.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Gates/WarpTransitionComponent.h"
#include "HomeRespawnSubsystem.h"
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

	/** A dodging pilot reacts this long after a telegraph starts (a quick human). */
	constexpr float DodgeReaction = 0.3f;

	/** A non-circling pilot strafes this long to dodge: past the telegraph and the shot's flight. */
	constexpr float DodgeStrafeSeconds = 2.5f;

	/** A circling pilot swaps strafe direction after a random time in this range. */
	constexpr float StrafeSwapMin = 3.f;
	constexpr float StrafeSwapMax = 5.f;

	const TCHAR* StyleName(const EPilotStyle Style)
	{
		return Style == EPilotStyle::Circle ? TEXT("circle") : Style == EPilotStyle::BackOff ? TEXT("backoff")
			: Style == EPilotStyle::Hold ? TEXT("hold") : TEXT("face");
	}
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
	UChargedShotSubsystem::OnAnyChargeStarted.Remove(ChargeHandle);
	Super::Deinitialize();
}

void UCombatTestPilot::Start(const FString& Type, const float Seconds, const float DistanceCm, const FName Sector)
{
	WantedType = Type;
	Remaining = Seconds;
	FightDistance = DistanceCm;
	Kills = 0;
	Deaths = 0;
	bWasDead = false;
	WarpRetryIn = 0.f;
	const USectorDirectorSubsystem* const Director = GetWorld()->GetSubsystem<USectorDirectorSubsystem>();
	FightSector = !Sector.IsNone() ? Sector : (Director ? Director->GetCurrentSectorId() : NAME_None);
	Dodges = 0;
	DodgeIn = -1.f;
	DodgeLeft = 0.f;
	StrafeSwapIn = 0.f;
	Target.Reset();
	Pilot = Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	bActive = Pilot.IsValid();
	if (!DeathHandle.IsValid())
	{
		DeathHandle = AStartechEnemyShip::OnAnyEnemyDied.AddUObject(this, &UCombatTestPilot::HandleEnemyDied);
	}
	if (!ChargeHandle.IsValid())
	{
		ChargeHandle = UChargedShotSubsystem::OnAnyChargeStarted.AddUObject(this, &UCombatTestPilot::HandleChargeStarted);
	}
	UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] %s: fighting %s for %.0f s at %.0f m, style %s%s."),
		bActive ? TEXT("started") : TEXT("no player ship"), *Type, Seconds, DistanceCm / 100.f,
		CombatTestPilot::StyleName(Style), bDodge ? TEXT(", dodging") : TEXT(""));
}

bool UCombatTestPilot::ReturnToFight(AStartechShipPawn& Ship, const float DeltaTime)
{
	UWorld* const World = GetWorld();
	const UHomeRespawnSubsystem* const Respawn = World->GetSubsystem<UHomeRespawnSubsystem>();
	if (Ship.GetHealthComponent()->IsDead() || (Respawn && Respawn->IsRespawning()))
	{
		if (!bWasDead)
		{
			bWasDead = true;
			++Deaths;
			Target.Reset();
			Ship.SetFireHeld(false);
			Ship.SetSteerAxis(0.f);
			UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] died (%d so far); waiting for the respawn."), Deaths);
		}
		return true;
	}
	bWasDead = false;

	if (Ship.IsHubOpen())
	{
		LogStatus(Ship, nullptr, TEXT("hub open"));
		return true;
	}
	if (Ship.IsDocked())
	{
		Ship.UndockFromStation();
		UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] undocked."));
		return true;
	}

	// Back to the fight's sector after a respawn at home.
	const USectorDirectorSubsystem* const Director = World->GetSubsystem<USectorDirectorSubsystem>();
	UWarpTransitionComponent* const Warp = Ship.FindComponentByClass<UWarpTransitionComponent>();
	if (FightSector.IsNone() || !Director || !Warp || Director->GetCurrentSectorId() == FightSector)
	{
		return false;
	}
	WarpRetryIn -= DeltaTime;
	if (!Warp->IsWarping() && !Director->IsBusy() && WarpRetryIn <= 0.f)
	{
		WarpRetryIn = 2.f;
		const bool bStarted = Warp->BeginWarp(FightSector);
		UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] warping back to %s: %s."), *FightSector.ToString(),
			bStarted ? TEXT("started") : TEXT("refused, retrying"));
	}
	return true;
}

void UCombatTestPilot::SetStyle(const EPilotStyle InStyle, const bool bInDodge, const float InBackOffThrottle)
{
	Style = InStyle;
	bDodge = bInDodge;
	BackOffThrottle = FMath::Clamp(InBackOffThrottle, 0.f, 1.f);
	UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] style %s%s%s."), CombatTestPilot::StyleName(Style),
		bDodge ? TEXT(", dodging charged shots") : TEXT(""),
		Style == EPilotStyle::BackOff ? *FString::Printf(TEXT(", reverse throttle %.2f"), BackOffThrottle) : TEXT(""));
}

void UCombatTestPilot::HandleChargeStarted(const UEnemyChargeComponent& Charge)
{
	if (bActive && bDodge && Charge.GetTarget() == Pilot.Get() && DodgeIn < 0.f)
	{
		DodgeIn = CombatTestPilot::DodgeReaction;
	}
}

float UCombatTestPilot::UpdateSteer(const float DeltaTime)
{
	if (DodgeIn >= 0.f)
	{
		DodgeIn -= DeltaTime;
		if (DodgeIn < 0.f)
		{
			++Dodges;
			if (Style == EPilotStyle::Circle)
			{
				// Flip, and hold the new direction past the shot.
				StrafeDirection = -StrafeDirection;
				StrafeSwapIn = FMath::Max(StrafeSwapIn, CombatTestPilot::DodgeStrafeSeconds);
			}
			else
			{
				StrafeDirection = FMath::RandBool() ? 1.f : -1.f;
				DodgeLeft = CombatTestPilot::DodgeStrafeSeconds;
			}
		}
	}

	if (Style == EPilotStyle::Circle)
	{
		StrafeSwapIn -= DeltaTime;
		if (StrafeSwapIn <= 0.f)
		{
			StrafeDirection = -StrafeDirection;
			StrafeSwapIn = FMath::FRandRange(CombatTestPilot::StrafeSwapMin, CombatTestPilot::StrafeSwapMax);
		}
		return StrafeDirection;
	}

	DodgeLeft = FMath::Max(DodgeLeft - DeltaTime, 0.f);
	return DodgeLeft > 0.f ? StrafeDirection : 0.f;
}

void UCombatTestPilot::Stop(const TCHAR* const Reason)
{
	if (AStartechShipPawn* const Ship = Pilot.Get())
	{
		Ship->SetFireHeld(false);
		Ship->SetThrottleInput(0.f);
		Ship->SetSteerAxis(0.f);
		if (UTargetingComponent* const Targeting = Ship->GetTargeting())
		{
			Targeting->ClearTarget();
		}
	}
	if (bActive)
	{
		UE_LOG(LogStartechPilot, Warning, TEXT("[Pilot] stopped (%s) after %d kill(s), %d dodge(s), %d death(s)."), Reason, Kills,
			Dodges, Deaths);
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
	if (!Ship || !Ship->GetHealthComponent())
	{
		Stop(TEXT("no player ship"));
		return;
	}
	Remaining -= DeltaTime;
	if (Remaining <= 0.f)
	{
		Stop(TEXT("time up"));
		return;
	}
	if (ReturnToFight(*Ship, DeltaTime))
	{
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
		bHolding = false;
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
	// Backed out of the sector: a real player would turn back before the out-of-bounds timer kills them; the pilot
	// jumps back to the fight instead.
	if (Distance > CombatTestPilot::LockDistance || Ship->IsOutsideSector())
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
	bHolding |= Style == EPilotStyle::Hold && Distance <= FightDistance + CombatTestPilot::DistanceBand;
	Ship->SetThrottleInput(Style == EPilotStyle::BackOff ? -BackOffThrottle : bHolding ? 0.f
		: Distance > FightDistance + CombatTestPilot::DistanceBand ? 1.f
		: Distance < FightDistance - CombatTestPilot::DistanceBand ? -1.f : 0.f);
	Ship->SetSteerAxis(UpdateSteer(DeltaTime));

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
		Pilot->Start(Args[0], Seconds, DistanceM * 100.f, Args.IsValidIndex(4) ? FName(*Args[4]) : NAME_None);
	}

	/** Startech.Test.PilotStyle <face|circle|backoff> [Dodge=0|1] [BackOffThrottle=1] */
	static void PilotStyle(const TArray<FString>& Args, UWorld* const World)
	{
		UCombatTestPilot* const Pilot = World ? World->GetSubsystem<UCombatTestPilot>() : nullptr;
		if (!Pilot || Args.IsEmpty())
		{
			return;
		}
		const EPilotStyle Style = Args[0].Equals(TEXT("circle"), ESearchCase::IgnoreCase) ? EPilotStyle::Circle
			: Args[0].Equals(TEXT("backoff"), ESearchCase::IgnoreCase) ? EPilotStyle::BackOff
			: Args[0].Equals(TEXT("hold"), ESearchCase::IgnoreCase) ? EPilotStyle::Hold : EPilotStyle::Face;
		Pilot->SetStyle(Style, Args.IsValidIndex(1) && FCString::Atoi(*Args[1]) != 0,
			Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 1.f);
	}

	static FAutoConsoleCommandWithWorldAndArgs PilotStyleCommand(TEXT("Startech.Test.PilotStyle"),
		TEXT("Startech.Test.PilotStyle <face|circle|backoff|hold> [Dodge=0|1] [BackOffThrottle=1]: how the test pilot flies its fights: hold the distance, hold it while strafing, reverse away at that throttle, or never touch the throttle; with Dodge it changes its strafe 0.3 s after a charged-shot telegraph starts against it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PilotStyle));

	static FAutoConsoleCommandWithWorldAndArgs AutoFightCommand(TEXT("Startech.Test.AutoFight"),
		TEXT("Startech.Test.AutoFight <Type|Any|stop> [Seconds=120] [DistanceM=60] [AmmoTag] [Sector]: flies the player into combat with the nearest enemy of Type (Drifter, Chaser, Ranged) until the time is up. After a death it respawns, undocks and warps back to Sector (default: the current one) by itself."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AutoFight));
}

#endif
