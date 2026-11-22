#include "Upgrades/UpgradeStatFormat.h"

#include "StartechGameplayTags.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Upgrades/ShipUpgradeStatsComponent.h"
#include "Upgrades/ShipUpgradeSubsystem.h"

#define LOCTEXT_NAMESPACE "UpgradeStatFormat"

namespace UpgradeStatFormat
{
	constexpr float CentimetresPerMetre = 100.f;

	/** Scale stats are shown as a percentage of this. */
	constexpr float PercentBase = 100.f;
}

float FUpgradeStatFormat::GetDisplayValue(const FUpgradeStatEffect& Effect, const FName TrackId, const int32 Level,
	const AStartechShipPawn& Ship, const UShipUpgradeSubsystem& Upgrades)
{
	const FGameplayTag& Tag = Effect.StatTag;
	const UShipUpgradeStatsComponent* const Stats = Ship.GetUpgradeStats();
	const FShipBoostSettings& Boost = UStartechSettings::Get().Boost;

	// The stat's base in display units. Scale stats (engines, weapons) multiply a ship value or 100%.
	float Base = UpgradeStatFormat::PercentBase;
	float ScaleOf = 1.f;
	bool bScale = true;
	if (Tag == StartechTags::Stat_Hull_Max)            { Base = Stats ? Stats->GetBaseMaxHull() : 0.f; bScale = false; }
	else if (Tag == StartechTags::Stat_Shield_Max)     { Base = Stats ? Stats->GetBaseMaxShield() : 0.f; bScale = false; }
	else if (Tag == StartechTags::Stat_Shield_Regen)   { Base = Stats ? Stats->GetBaseShieldRegen() : 0.f; bScale = false; }
	else if (Tag == StartechTags::Stat_Cargo_Capacity) { Base = Stats ? Stats->GetBaseCargoCapacity() : 0.f; bScale = false; }
	else if (Tag == StartechTags::Stat_Engine_Speed)
	{
		const float Scale = FMath::Max(Ship.GetNormalSpeedScale(), KINDA_SMALL_NUMBER);
		ScaleOf = Ship.GetNormalMaxForwardSpeed() / Scale / UpgradeStatFormat::CentimetresPerMetre;
	}
	else if (Tag == StartechTags::Stat_Boost_Energy)   { ScaleOf = Boost.MaxBoostEnergy; }
	else if (Tag == StartechTags::Stat_Boost_Regen)    { ScaleOf = Boost.RegenPerSecond; }
	else                                               { ScaleOf = UpgradeStatFormat::PercentBase; }

	if (bScale)
	{
		// The scale is (1 + flat) × multiplier, as UShipUpgradeStatsComponent sets it.
		Base = ScaleOf;
		return Base * (1.f + Upgrades.GetStatBonusWith(Tag, TrackId, Level)) * Upgrades.GetStatMultiplierWith(Tag, TrackId, Level);
	}
	return (Base + Upgrades.GetStatBonusWith(Tag, TrackId, Level)) * Upgrades.GetStatMultiplierWith(Tag, TrackId, Level);
}

FText FUpgradeStatFormat::FormatNumber(const FUpgradeStatEffect& Effect, const float Value)
{
	const FGameplayTag& Tag = Effect.StatTag;
	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMaximumFractionalDigits(1);

	if (Tag == StartechTags::Stat_Shield_Regen || Tag == StartechTags::Stat_Boost_Regen)
	{
		return FText::Format(LOCTEXT("PerSecond", "{0}/s"), FText::AsNumber(Value, &OneDecimal));
	}
	if (Tag == StartechTags::Stat_Engine_Speed)
	{
		return FText::Format(LOCTEXT("Speed", "{0} m/s"), FText::AsNumber(Value, &OneDecimal));
	}
	const bool bWholeUnits = Tag == StartechTags::Stat_Hull_Max || Tag == StartechTags::Stat_Shield_Max
		|| Tag == StartechTags::Stat_Cargo_Capacity || Tag == StartechTags::Stat_Boost_Energy;
	if (bWholeUnits)
	{
		return FText::AsNumber(FMath::RoundToInt(Value));
	}

	// Laser damage, drainer power and any stat without a unit of its own: a percentage of the base.
	return FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(FMath::RoundToInt(Value)));
}

FText FUpgradeStatFormat::FormatChange(const FUpgradeStatEffect& Effect, const FName TrackId, const int32 FromLevel,
	const int32 ToLevel, const AStartechShipPawn& Ship, const UShipUpgradeSubsystem& Upgrades)
{
	return FText::Format(LOCTEXT("Change", "{0} {1} → {2}"), Effect.StatLabel,
		FormatNumber(Effect, GetDisplayValue(Effect, TrackId, FromLevel, Ship, Upgrades)),
		FormatNumber(Effect, GetDisplayValue(Effect, TrackId, ToLevel, Ship, Upgrades)));
}

FText FUpgradeStatFormat::FormatValue(const FUpgradeStatEffect& Effect, const FName TrackId, const int32 Level,
	const AStartechShipPawn& Ship, const UShipUpgradeSubsystem& Upgrades)
{
	return FText::Format(LOCTEXT("Value", "{0} {1}"), Effect.StatLabel,
		FormatNumber(Effect, GetDisplayValue(Effect, TrackId, Level, Ship, Upgrades)));
}

#undef LOCTEXT_NAMESPACE
