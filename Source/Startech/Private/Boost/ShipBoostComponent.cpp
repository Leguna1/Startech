#include "Boost/ShipBoostComponent.h"

#include "Boost/ShipBoostSettings.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechBoost, Log, All);

UShipBoostComponent::UShipBoostComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const FShipBoostSettings& UShipBoostComponent::GetSettings()
{
	return UStartechSettings::Get().Boost;
}

void UShipBoostComponent::BeginPlay()
{
	Super::BeginPlay();
	Refill();
}

float UShipBoostComponent::GetMaxEnergy() const
{
	return FMath::Max(GetSettings().MaxBoostEnergy * MaxEnergyScale, 1.f);
}

float UShipBoostComponent::GetRegenPerSecond() const
{
	return GetSettings().RegenPerSecond * RegenScale;
}

float UShipBoostComponent::GetRestartThreshold() const
{
	return GetMaxEnergy() * GetSettings().RestartFraction;
}

float UShipBoostComponent::GetBoostAlpha() const
{
	const float Range = GetSettings().MaxSpeedMultiplier - 1.f;
	return Range > 0.f ? FMath::Clamp((SpeedMultiplier - 1.f) / Range, 0.f, 1.f) : 0.f;
}

void UShipBoostComponent::Refill()
{
	bHeld = false;
	SetBoosting(false);
	bLockedOut = false;
	SpeedMultiplier = 1.f;
	Energy = GetMaxEnergy();
	TimeSinceBoostStopped = GetSettings().RegenDelaySeconds;
	LastBroadcastEnergy = -1.f;
	NotifyEnergy(true);
}

void UShipBoostComponent::StopImmediately()
{
	bHeld = false;
	SetBoosting(false);
	SpeedMultiplier = 1.f;
}

void UShipBoostComponent::SetEnergyScales(const float InMaxEnergyScale, const float InRegenScale)
{
	const float OldMax = GetMaxEnergy();
	MaxEnergyScale = FMath::Max(InMaxEnergyScale, 0.01f);
	RegenScale = FMath::Max(InRegenScale, 0.f);

	// A bigger pool never feels like losing energy: the current value rises by the same amount.
	const float NewMax = GetMaxEnergy();
	Energy = FMath::Clamp(Energy + FMath::Max(NewMax - OldMax, 0.f), 0.f, NewMax);
	NotifyEnergy(true);
}

void UShipBoostComponent::SetBoostHeld(const bool bInHeld)
{
	if (!bInHeld)
	{
		bHeld = false;
		return;
	}

	if (bHeld)
	{
		return;
	}
	bHeld = true;

	if (bLockedOut || Energy <= 0.f)
	{
		UE_LOG(LogStartechBoost, Log, TEXT("Boost refused: %.1f / %.1f energy, restart at %.1f."), Energy, GetMaxEnergy(),
			GetRestartThreshold());
		OnBoostDenied.Broadcast();
		return;
	}

	SetBoosting(true);
}

void UShipBoostComponent::UpdateBoost(const float DeltaSeconds, const bool bAllowed)
{
	const FShipBoostSettings& Settings = GetSettings();
	const float MaxEnergy = GetMaxEnergy();
	const float Threshold = GetRestartThreshold();
	const bool bWasBelowThreshold = Energy < Threshold;

	if (!bAllowed)
	{
		bHeld = false;
	}

	if (bBoosting && !bHeld)
	{
		SetBoosting(false);
	}

	bool bForceEvent = false;
	if (bBoosting)
	{
		Energy = FMath::Max(Energy - Settings.DrainPerSecond * DeltaSeconds, 0.f);
		if (Energy <= 0.f)
		{
			bLockedOut = true;
			bForceEvent = true;
			UE_LOG(LogStartechBoost, Log, TEXT("Boost energy empty: locked out until %.1f."), Threshold);
			SetBoosting(false);
		}
	}
	else
	{
		TimeSinceBoostStopped += DeltaSeconds;
		if (TimeSinceBoostStopped >= Settings.RegenDelaySeconds && Energy < MaxEnergy)
		{
			Energy = FMath::Min(Energy + GetRegenPerSecond() * DeltaSeconds, MaxEnergy);
			bForceEvent |= Energy >= MaxEnergy;
		}

		if (bLockedOut && Energy >= Threshold)
		{
			bLockedOut = false;
			UE_LOG(LogStartechBoost, Log, TEXT("Boost available again at %.1f energy."), Energy);
		}
	}

	// Crossing the restart threshold changes the gauge's colour, so it is never held back by the throttle.
	bForceEvent |= bWasBelowThreshold != (Energy < Threshold);

	const float Range = Settings.MaxSpeedMultiplier - 1.f;
	const float Target = bBoosting ? Settings.MaxSpeedMultiplier : 1.f;
	const float Rate = Range / (bBoosting ? Settings.RampUpSeconds : Settings.RampDownSeconds);
	SpeedMultiplier = FMath::FInterpConstantTo(SpeedMultiplier, Target, DeltaSeconds, Rate);

	TimeSinceEnergyEvent += DeltaSeconds;
	NotifyEnergy(bForceEvent);
}

void UShipBoostComponent::SetBoosting(const bool bNewBoosting)
{
	if (bBoosting == bNewBoosting)
	{
		return;
	}

	bBoosting = bNewBoosting;
	if (!bBoosting)
	{
		TimeSinceBoostStopped = 0.f;
	}

	UE_LOG(LogStartechBoost, Log, TEXT("Boost %s at %.1f / %.1f energy."), bBoosting ? TEXT("started") : TEXT("stopped"),
		Energy, GetMaxEnergy());
	OnBoostStateChanged.Broadcast(bBoosting);
}

void UShipBoostComponent::NotifyEnergy(const bool bForce)
{
	const float MaxEnergy = GetMaxEnergy();
	if (Energy == LastBroadcastEnergy && MaxEnergy == LastBroadcastMax)
	{
		return;
	}

	if (!bForce && TimeSinceEnergyEvent < GetSettings().EnergyEventInterval)
	{
		return;
	}

	TimeSinceEnergyEvent = 0.f;
	LastBroadcastEnergy = Energy;
	LastBroadcastMax = MaxEnergy;
	OnBoostEnergyChanged.Broadcast(Energy, MaxEnergy);
}
