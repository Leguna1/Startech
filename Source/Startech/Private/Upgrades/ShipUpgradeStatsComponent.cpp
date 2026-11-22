#include "Upgrades/ShipUpgradeStatsComponent.h"

#include "Boost/ShipBoostComponent.h"
#include "HealthComponent.h"
#include "InventoryComponent.h"
#include "StartechGameplayTags.h"
#include "StartechShipPawn.h"
#include "Upgrades/ShipUpgradeSubsystem.h"
#include "WeaponMountComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechUpgradeStats, Log, All);

UShipUpgradeStatsComponent::UShipUpgradeStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AStartechShipPawn* UShipUpgradeStatsComponent::GetShip() const
{
	return Cast<AStartechShipPawn>(GetOwner());
}

void UShipUpgradeStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AStartechShipPawn* const Ship = GetShip())
	{
		const UHealthComponent* const Health = Ship->GetHealthComponent();
		BaseMaxHull = Health->GetMaxHull();
		BaseMaxShield = Health->GetMaxShield();
		BaseShieldRegen = Health->GetShieldRegenRate();
		BaseCargoCapacity = Ship->GetInventory()->GetCargoCapacity();
		bHasBase = true;
	}

	if (UShipUpgradeSubsystem* const Upgrades = UShipUpgradeSubsystem::Get(this))
	{
		Upgrades->OnUpgradeChanged.AddUniqueDynamic(this, &UShipUpgradeStatsComponent::HandleUpgradeChanged);
	}
	ApplyStats();
}

void UShipUpgradeStatsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UShipUpgradeSubsystem* const Upgrades = UShipUpgradeSubsystem::Get(this))
	{
		Upgrades->OnUpgradeChanged.RemoveDynamic(this, &UShipUpgradeStatsComponent::HandleUpgradeChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UShipUpgradeStatsComponent::HandleUpgradeChanged(const FName TrackId, const int32 NewLevel)
{
	ApplyStats();

	// Upgrades are bought docked, and docking means a full repair: the new maximums are filled at once. Undocked
	// changes (test commands) keep SetMaximums' rule of raising the current value by the same amount.
	const AStartechShipPawn* const Ship = GetShip();
	if (Ship && Ship->IsDocked())
	{
		UHealthComponent* const Health = Ship->GetHealthComponent();
		Health->RepairHull(Health->GetMaxHull());
		Health->RestoreShield(Health->GetMaxShield());
	}
}

float UShipUpgradeStatsComponent::GetUpgradedValue(const FGameplayTag& StatTag, const float Base) const
{
	const UShipUpgradeSubsystem* const Upgrades = UShipUpgradeSubsystem::Get(this);
	return Upgrades ? (Base + Upgrades->GetStatBonus(StatTag)) * Upgrades->GetStatMultiplier(StatTag) : Base;
}

void UShipUpgradeStatsComponent::ApplyStats()
{
	AStartechShipPawn* const Ship = GetShip();
	if (!Ship || !bHasBase)
	{
		return;
	}

	UHealthComponent* const Health = Ship->GetHealthComponent();
	Health->SetMaximums(GetUpgradedValue(StartechTags::Stat_Hull_Max, BaseMaxHull),
		GetUpgradedValue(StartechTags::Stat_Shield_Max, BaseMaxShield));
	Health->SetShieldRegenRate(GetUpgradedValue(StartechTags::Stat_Shield_Regen, BaseShieldRegen));

	Ship->GetInventory()->SetCargoCapacity(
		FMath::RoundToInt(GetUpgradedValue(StartechTags::Stat_Cargo_Capacity, static_cast<float>(BaseCargoCapacity))));

	Ship->SetFlightScales(GetUpgradedValue(StartechTags::Stat_Engine_Speed, 1.f), Ship->GetBoostSpeedScale());

	const float LaserScale = GetUpgradedValue(StartechTags::Stat_Laser_Damage, 1.f);
	const float DrainerScale = GetUpgradedValue(StartechTags::Stat_Drainer_Power, 1.f);
	TInlineComponentArray<UWeaponMountComponent*> Mounts(Ship);
	for (UWeaponMountComponent* const Mount : Mounts)
	{
		Mount->SetDamageScale(Mount->GetGroup() == EWeaponGroup::ShieldDrain ? DrainerScale : LaserScale);
	}

	if (UShipBoostComponent* const Boost = Ship->GetBoost())
	{
		Boost->SetEnergyScales(GetUpgradedValue(StartechTags::Stat_Boost_Energy, 1.f),
			GetUpgradedValue(StartechTags::Stat_Boost_Regen, 1.f));
	}

	UE_LOG(LogStartechUpgradeStats, Log,
		TEXT("Upgrade stats applied: hull %.0f shield %.0f regen %.1f/s cargo %d speed x%.2f laser x%.2f drainer x%.2f boost %.0f (+%.1f/s)"),
		Health->GetMaxHull(), Health->GetMaxShield(), Health->GetShieldRegenRate(), Ship->GetInventory()->GetCargoCapacity(),
		Ship->GetNormalSpeedScale(), LaserScale, DrainerScale, Ship->GetBoost() ? Ship->GetBoost()->GetMaxEnergy() : 0.f,
		Ship->GetBoost() ? Ship->GetBoost()->GetRegenPerSecond() : 0.f);
}
