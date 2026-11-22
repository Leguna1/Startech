#include "HealthComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "ShieldDrainDamageType.h"
#include "StartechHUDLog.h"
#include "TimerManager.h"

namespace HealthComponent
{
	// A pending report this close to its due time is sent now rather than rescheduled for a sliver.
	constexpr double ReportTolerance = 0.001;

	// Shortest flush delay, so a zero interval cannot schedule a timer for the same instant.
	constexpr float MinFlushDelay = 0.01f;
}

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

FOnAnyHealthDamage UHealthComponent::OnAnyDamageApplied;

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	const FHealthSnapshot Before = TakeSnapshot();

	Hull = MaxHull;
	Shield = MaxShield;
	TimeSinceDamage = 0.f;
	bDead = false;

	if (AActor* const Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
	}

	// Hull and shield only hold real values from here on. Anything that bound earlier (the HUD can be created
	// before the pawn begins play) gets the starting values now instead of waiting for the first damage.
	NotifyValuesChanged(Before, false);
}

void UHealthComponent::InitializeHealth(const float InMaxHull, const float InMaxShield)
{
	const FHealthSnapshot Before = TakeSnapshot();

	MaxHull = FMath::Max(InMaxHull, 0.f);
	MaxShield = FMath::Max(InMaxShield, 0.f);
	Hull = MaxHull;
	Shield = MaxShield;

	NotifyValuesChanged(Before, false);
}

void UHealthComponent::SetMaximums(const float InMaxHull, const float InMaxShield)
{
	const FHealthSnapshot Before = TakeSnapshot();

	const float NewMaxHull = FMath::Max(InMaxHull, 0.f);
	const float NewMaxShield = FMath::Max(InMaxShield, 0.f);
	if (!bDead)
	{
		Hull = FMath::Clamp(Hull + FMath::Max(NewMaxHull - MaxHull, 0.f), 0.f, NewMaxHull);
	}
	Shield = FMath::Clamp(Shield + FMath::Max(NewMaxShield - MaxShield, 0.f), 0.f, NewMaxShield);
	MaxHull = NewMaxHull;
	MaxShield = NewMaxShield;

	NotifyValuesChanged(Before, false);
}

void UHealthComponent::RepairHull(const float Amount)
{
	if (bDead || Amount <= 0.f || Hull >= MaxHull)
	{
		return;
	}

	const FHealthSnapshot Before = TakeSnapshot();
	Hull = FMath::Min(Hull + Amount, MaxHull);
	NotifyValuesChanged(Before, true);
}

void UHealthComponent::RestoreShield(const float Amount)
{
	if (bDead || Amount <= 0.f || Shield >= MaxShield)
	{
		return;
	}

	const FHealthSnapshot Before = TakeSnapshot();
	Shield = FMath::Min(Shield + Amount, MaxShield);
	NotifyValuesChanged(Before, true);
}

void UHealthComponent::Revive()
{
	const FHealthSnapshot Before = TakeSnapshot();

	Hull = MaxHull;
	Shield = MaxShield;
	TimeSinceDamage = 0.f;
	bDead = false;

	// A new life starts with nothing left to report from the last one.
	DamageReports.Reset();
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageFlushTimer);
	}

	NotifyValuesChanged(Before, false);
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* const Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
	}

	Super::EndPlay(EndPlayReason);
}

void UHealthComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDead)
	{
		return;
	}

	TimeSinceDamage += DeltaTime;

	if (Shield >= MaxShield || TimeSinceDamage < ShieldRegenDelay)
	{
		return;
	}

	const FHealthSnapshot Before = TakeSnapshot();
	Shield = FMath::Min(Shield + ShieldRegenRate * DeltaTime, MaxShield);
	NotifyValuesChanged(Before, true);
}

void UHealthComponent::HandleTakeAnyDamage(AActor* const DamagedActor, const float Damage,
	const UDamageType* const DamageType, AController* const InstigatedBy, AActor* const DamageCauser)
{
	if (bDead || bInvulnerable || Damage <= 0.f)
	{
		return;
	}

	const float ShieldDamage = FMath::Min(Damage, Shield);

	// Drain damage stops at the shield; anything left over is discarded.
	const bool bShieldOnly = DamageType && DamageType->IsA<UShieldDrainDamageType>();
	const float HullDamage = bShieldOnly ? 0.f : FMath::Min(Damage - ShieldDamage, Hull);

	// The causer is the firing ship. Without one (e.g. leaving the sector), the hit has no direction.
	const AActor* const Owner = GetOwner();
	const FVector SourceLocation = DamageCauser
		? DamageCauser->GetActorLocation()
		: (Owner ? Owner->GetActorLocation() : FVector::ZeroVector);

	// Prefer the instigating pawn; fall back to whatever caused the damage.
	APawn* const InstigatorPawn = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
	AActor* const Killer = InstigatorPawn ? static_cast<AActor*>(InstigatorPawn) : DamageCauser;

	ApplySplitDamage(ShieldDamage, HullDamage, DamageCauser, SourceLocation, Killer, bShieldOnly);
}

bool UHealthComponent::ApplyDirectDamage(const float ShieldAmount, const float HullAmount, const FVector& SourceLocation)
{
	if (bDead || bInvulnerable)
	{
		STARTECH_HUD_LOG(TEXT("%s ignored direct damage: %s"), *GetNameSafe(GetOwner()),
			bDead ? TEXT("dead") : TEXT("invulnerable (docked?)"));
		return false;
	}

	const float ShieldDamage = FMath::Clamp(ShieldAmount, 0.f, Shield);
	const float HullDamage = FMath::Clamp(HullAmount, 0.f, Hull);
	ApplySplitDamage(ShieldDamage, HullDamage, nullptr, SourceLocation, nullptr);
	return true;
}

void UHealthComponent::ApplySplitDamage(const float ShieldDamage, const float HullDamage, const AActor* const Source,
	const FVector& SourceLocation, AActor* const Killer, const bool bShieldDrain)
{
	TimeSinceDamage = 0.f;

	const FHealthSnapshot Before = TakeSnapshot();
	Shield = FMath::Max(Shield - ShieldDamage, 0.f);
	Hull = FMath::Max(Hull - HullDamage, 0.f);

	NotifyValuesChanged(Before, false);

	if (ShieldDamage > 0.f)
	{
		ReportDamage(ShieldDamage, true, Source, SourceLocation);
	}
	if (HullDamage > 0.f)
	{
		ReportDamage(HullDamage, false, Source, SourceLocation);
	}

	if (ShieldDamage > 0.f || HullDamage > 0.f)
	{
		OnAnyDamageApplied.Broadcast(*this, ShieldDamage, HullDamage, Killer, bShieldDrain, Hull <= 0.f);
	}

	if (Hull <= 0.f)
	{
		bDead = true;

		// The killing blow is reported before the death, whatever its interval.
		FlushAllDamageReports();
		OnDeath.Broadcast(Killer);
	}
}

UHealthComponent::FHealthSnapshot UHealthComponent::TakeSnapshot() const
{
	FHealthSnapshot Snapshot;
	Snapshot.Hull = Hull;
	Snapshot.Shield = Shield;
	Snapshot.MaxHull = MaxHull;
	Snapshot.MaxShield = MaxShield;
	return Snapshot;
}

void UHealthComponent::NotifyValuesChanged(const FHealthSnapshot& Before, const bool bContinuous)
{
	OnHealthChanged.Broadcast(Hull, Shield);

	const FString OwnerName = GetNameSafe(GetOwner());

	if (Shield != Before.Shield || MaxShield != Before.MaxShield)
	{
		if (bContinuous)
		{
			STARTECH_HUD_LOG_VERBOSE(TEXT("%s OnShieldChanged: %.1f / %.1f"), *OwnerName, Shield, MaxShield);
		}
		else
		{
			STARTECH_HUD_LOG(TEXT("%s OnShieldChanged: %.1f / %.1f"), *OwnerName, Shield, MaxShield);
		}
		OnShieldChanged.Broadcast(Shield, MaxShield);
	}

	if (Hull != Before.Hull || MaxHull != Before.MaxHull)
	{
		if (bContinuous)
		{
			STARTECH_HUD_LOG_VERBOSE(TEXT("%s OnHullChanged: %.1f / %.1f"), *OwnerName, Hull, MaxHull);
		}
		else
		{
			STARTECH_HUD_LOG(TEXT("%s OnHullChanged: %.1f / %.1f"), *OwnerName, Hull, MaxHull);
		}
		OnHullChanged.Broadcast(Hull, MaxHull);
	}

	UpdateLowHull();
}

void UHealthComponent::UpdateLowHull()
{
	const bool bNowLow = MaxHull > 0.f && Hull < MaxHull * LowHullFraction;
	if (bNowLow == bLowHull)
	{
		return;
	}

	bLowHull = bNowLow;
	STARTECH_HUD_LOG(TEXT("%s OnLowHullChanged: %s"), *GetNameSafe(GetOwner()), bLowHull ? TEXT("true") : TEXT("false"));
	OnLowHullChanged.Broadcast(bLowHull);
}

void UHealthComponent::ReportDamage(const float Amount, const bool bHitShield, const AActor* const Source,
	const FVector& SourceLocation)
{
	const double Now = GetNow();

	FDamageReport* Report = DamageReports.FindByPredicate([Source, bHitShield](const FDamageReport& Each)
	{
		return Each.bHitShield == bHitShield && Each.Source.Get() == Source;
	});
	if (!Report)
	{
		Report = &DamageReports.AddDefaulted_GetRef();
		Report->Source = Source;
		Report->bHitShield = bHitShield;
	}

	Report->PendingAmount += Amount;
	Report->SourceLocation = SourceLocation;

	if (Now - Report->LastReportTime >= DamageReportInterval - HealthComponent::ReportTolerance)
	{
		BroadcastReport(*Report, Now);
	}
	else
	{
		ScheduleDamageFlush();
	}
}

void UHealthComponent::BroadcastReport(FDamageReport& Report, const double Now)
{
	const float Amount = Report.PendingAmount;
	Report.PendingAmount = 0.f;
	Report.LastReportTime = Now;

	STARTECH_HUD_LOG(TEXT("%s OnDamageTaken: %.1f to %s from %s"), *GetNameSafe(GetOwner()), Amount,
		Report.bHitShield ? TEXT("shield") : TEXT("hull"), *Report.SourceLocation.ToCompactString());
	OnDamageTaken.Broadcast(Amount, Report.bHitShield, Report.SourceLocation);
}

void UHealthComponent::FlushDueDamageReports()
{
	const double Now = GetNow();

	// Indexed: a listener could damage this ship again and add an entry mid-loop.
	for (int32 Index = 0; Index < DamageReports.Num(); ++Index)
	{
		FDamageReport& Report = DamageReports[Index];
		if (Report.PendingAmount > 0.f
			&& Now - Report.LastReportTime >= DamageReportInterval - HealthComponent::ReportTolerance)
		{
			BroadcastReport(Report, Now);
		}
	}

	// Entries with nothing pending and their interval over hold no state worth keeping.
	DamageReports.RemoveAllSwap([this, Now](const FDamageReport& Report)
	{
		return Report.PendingAmount <= 0.f && Now - Report.LastReportTime >= DamageReportInterval;
	});

	ScheduleDamageFlush();
}

void UHealthComponent::FlushAllDamageReports()
{
	const double Now = GetNow();
	for (int32 Index = 0; Index < DamageReports.Num(); ++Index)
	{
		if (DamageReports[Index].PendingAmount > 0.f)
		{
			BroadcastReport(DamageReports[Index], Now);
		}
	}

	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageFlushTimer);
	}
}

void UHealthComponent::ScheduleDamageFlush()
{
	UWorld* const World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(DamageFlushTimer))
	{
		return;
	}

	const double Now = GetNow();
	double EarliestDue = TNumericLimits<double>::Max();
	for (const FDamageReport& Report : DamageReports)
	{
		if (Report.PendingAmount > 0.f)
		{
			EarliestDue = FMath::Min(EarliestDue, Report.LastReportTime + DamageReportInterval);
		}
	}

	if (EarliestDue == TNumericLimits<double>::Max())
	{
		return;
	}

	const float Delay = FMath::Max(static_cast<float>(EarliestDue - Now), HealthComponent::MinFlushDelay);
	World->GetTimerManager().SetTimer(DamageFlushTimer, this, &UHealthComponent::FlushDueDamageReports, Delay, false);
}

double UHealthComponent::GetNow() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
