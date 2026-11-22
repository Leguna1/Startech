#include "Combat/CombatTTKTable.h"

#include "AmmoData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EnemyBrainComponent.h"
#include "EnemyData.h"
#include "HealthComponent.h"
#include "ItemData.h"
#include "LaserMountComponent.h"
#include "LaserWeaponData.h"
#include "LootTable.h"
#include "ShopData.h"
#include "StartechEnemyShip.h"
#include "StartechGameplayTags.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Tiers/StartechTierSubsystem.h"
#include "Upgrades/UpgradeCatalog.h"

namespace CombatTTK
{
	/** Fighting three at once, killed one after another: three fire for the first kill, two, then one. */
	constexpr float ThreeAtOnceFactor = 6.f;

	/** Comparisons forgive float noise: a time that prints as 6.00 s is inside a 6 s target. */
	constexpr float TimeEpsilon = 0.005f;
	constexpr float LossEpsilon = 0.0005f;

	/** Base × (1 + the track's Percent bonus for Stat at Level) + its Flat bonus, as the stats component applies it. */
	float UpgradedValue(const UUpgradeCatalog* const Catalog, const FName TrackId, const int32 Level,
		const FGameplayTag& Stat, const float Base)
	{
		const FUpgradeTrack* const Track = Catalog ? Catalog->FindTrack(TrackId) : nullptr;
		float Flat = 0.f;
		float Percent = 0.f;
		if (Track)
		{
			for (const FUpgradeStatEffect& Effect : Track->Effects)
			{
				if (Effect.StatTag.MatchesTagExact(Stat))
				{
					(Effect.Kind == EUpgradeBonusKind::Flat ? Flat : Percent) += Effect.GetBonus(Level);
				}
			}
		}
		return (Base + Flat) * (1.f + Percent);
	}

	/** Average of an inclusive integer range; Max below Min counts as Min. */
	double Average(const int32 Min, const int32 Max)
	{
		return 0.5 * (Min + FMath::Max(Min, Max));
	}

	/** The average value of one roll of Table with Modifiers, as ULootTable::Roll rolls it. Ammo is worth nothing. */
	double ExpectedLootValue(const ULootTable& Table, const FLootRollModifiers& Modifiers, const UCombatBalance& Balance)
	{
		double Value = Table.CreditChance * Average(Table.CreditsMin, Table.CreditsMax) * Modifiers.CreditsMultiplier;

		float TotalWeight = 0.f;
		for (const FLootItemEntry& Entry : Table.Items)
		{
			if (Entry.Item && Entry.Item->ItemTag.MatchesTag(StartechTags::Item_Mineral))
			{
				const FTierMineralWeight* const Weight = Modifiers.MineralWeights.FindByPredicate(
					[&Entry](const FTierMineralWeight& Each) { return Each.Mineral.MatchesTagExact(Entry.Item->ItemTag); });
				TotalWeight += Weight ? FMath::Max(Weight->Weight, 0.f) : 0.f;
			}
		}

		for (const FLootItemEntry& Entry : Table.Items)
		{
			if (!Entry.Item)
			{
				continue;
			}
			const double Amount = Average(Entry.MinCount, Entry.MaxCount) * Modifiers.AmountMultiplier;
			const bool bMineral = Entry.Item->ItemTag.MatchesTag(StartechTags::Item_Mineral);
			if (bMineral && TotalWeight > 0.f)
			{
				const FTierMineralWeight* const Weight = Modifiers.MineralWeights.FindByPredicate(
					[&Entry](const FTierMineralWeight& Each) { return Each.Mineral.MatchesTagExact(Entry.Item->ItemTag); });
				const double Share = Weight ? FMath::Max(Weight->Weight, 0.f) / TotalWeight : 0.0;
				Value += Table.MineralChance * Share * Amount * Balance.GetSellValue(Entry.Item);
			}
			else if (!bMineral || TotalWeight <= 0.f)
			{
				Value += Entry.Chance * Amount * Balance.GetSellValue(Entry.Item);
			}
		}
		return Value;
	}

	/** Every UEnemyData asset, loaded. Scans first: commandlets and -game may not have scanned yet. */
	TArray<const UEnemyData*> LoadEnemyData()
	{
		IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		Registry.ScanPathsSynchronous({ TEXT("/Game") }, false);
		TArray<FAssetData> Found;
		Registry.GetAssetsByClass(UEnemyData::StaticClass()->GetClassPathName(), Found, true);

		TArray<const UEnemyData*> Result;
		for (const FAssetData& Asset : Found)
		{
			if (const UEnemyData* const Data = Cast<UEnemyData>(Asset.GetAsset()))
			{
				Result.Add(Data);
			}
		}
		return Result;
	}

	/** "LL3". */
	FString AmmoName(const UAmmoData* const Ammo)
	{
		return Ammo ? Ammo->DisplayName.ToString() : FString(TEXT("?"));
	}

	/** The last part of a tag: "Chaser" for Enemy.Chaser. */
	FString TagLeaf(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		int32 Dot = INDEX_NONE;
		return Name.FindLastChar(TEXT('.'), Dot) ? Name.RightChop(Dot + 1) : Name;
	}

	/** Value with a '*' when outside its target. */
	FString Mark(const FString& Value, const bool bOutside)
	{
		return bOutside ? Value + TEXT("*") : Value + TEXT(" ");
	}
}

FCombatTTKTable FCombatTTKTable::Build(const UCombatBalance& Balance)
{
	using namespace CombatTTK;

	FCombatTTKTable Table;
	const UStartechSettings& Settings = UStartechSettings::Get();

	// The player's lasers: the new-game laser mounts.
	const ULaserWeaponData* PlayerLaser = nullptr;
	int32 PlayerMounts = 0;
	for (const FStartechMountWeapon& Mount : Settings.NewGameMountWeapons)
	{
		if (const ULaserWeaponData* const Laser = Settings.FindItemOfType<ULaserWeaponData>(Mount.WeaponTag))
		{
			PlayerLaser = PlayerLaser ? PlayerLaser : Laser;
			++PlayerMounts;
		}
	}
	if (!PlayerLaser)
	{
		Table.Errors.Add(TEXT("UStartechSettings::NewGameMountWeapons has no laser."));
		return Table;
	}

	// The player's base health, from the ship Blueprint's health component.
	const UClass* const ShipClass = Balance.PlayerShipClass.LoadSynchronous();
	const AActor* const Ship = ShipClass ? ShipClass->GetDefaultObject<AActor>() : nullptr;
	const UHealthComponent* const ShipHealth = Ship ? Ship->FindComponentByClass<UHealthComponent>() : nullptr;
	if (!ShipHealth)
	{
		Table.Errors.Add(TEXT("DA_CombatBalance PlayerShipClass is not set or has no health component."));
		return Table;
	}

	const UUpgradeCatalog* const Catalog = Settings.UpgradeCatalog.LoadSynchronous();
	if (!Catalog)
	{
		Table.Errors.Add(TEXT("UStartechSettings::UpgradeCatalog is not set."));
	}
	const UShopData* const AmmoShop = Balance.AmmoShop.LoadSynchronous();
	const UShopData* const LootShop = Balance.LootShop.LoadSynchronous();
	if (!AmmoShop || !LootShop)
	{
		Table.Errors.Add(TEXT("DA_CombatBalance AmmoShop or LootShop is not set."));
	}

	// The enemies' own mounts: every laser mount on the enemy ship class.
	TArray<ULaserMountComponent*> EnemyMountList;
	GetDefault<AStartechEnemyShip>()->GetComponents(EnemyMountList);
	const int32 EnemyMounts = EnemyMountList.Num();
	const float RangeTolerance = GetDefault<UEnemyBrainComponent>()->GetRangeTolerance();

	const TArray<const UEnemyData*> AllEnemyData = LoadEnemyData();

	// Asset values that the balance replaces at runtime: noted when they differ, so the editor doesn't mislead.
	for (const FAmmoBalance& Entry : Balance.Ammo)
	{
		const UAmmoData* const Ammo = Settings.FindItemOfType<UAmmoData>(Entry.AmmoTag);
		if (!Ammo)
		{
			Table.Errors.Add(FString::Printf(TEXT("Ammo %s is not in the item catalog."), *Entry.AmmoTag.ToString()));
		}
		else if (!FMath::IsNearlyEqual(Ammo->DamageMultiplier, Entry.DamageMultiplier) || !FMath::IsNearlyEqual(Ammo->BaseValue, Entry.BaseValue))
		{
			Table.Notes.Add(FString::Printf(TEXT("%s asset has x%.2f / %.2f cr; DA_CombatBalance x%.2f / %.2f cr is used."),
				*AmmoName(Ammo), Ammo->DamageMultiplier, Ammo->BaseValue, Entry.DamageMultiplier, Entry.BaseValue));
		}
	}

	for (const FEnemyBalance& Enemy : Balance.Enemies)
	{
		const FString EnemyName = TagLeaf(Enemy.EnemyTag);
		const UEnemyData* const* const DataPtr = AllEnemyData.FindByPredicate([&Enemy](const UEnemyData* Data)
		{
			return Data->EnemyTags.HasTagExact(Enemy.EnemyTag);
		});
		const UEnemyData* const Data = DataPtr ? *DataPtr : nullptr;
		if (!Data || !Enemy.LaserWeapon)
		{
			Table.Errors.Add(FString::Printf(TEXT("%s: no UEnemyData with that tag, or no laser weapon."), *EnemyName));
			continue;
		}

		// The brain holds anywhere in PreferredRange ± tolerance; at the far edge the gun must still reach.
		const float FarthestHold = Enemy.PreferredRange * (1.f + RangeTolerance);
		if (FarthestHold > Enemy.LaserWeapon->Range)
		{
			Table.Errors.Add(FString::Printf(TEXT("%s: holds as far as %.0f m but its %s reaches %.0f m: it can't fire."),
				*EnemyName, FarthestHold / 100.f, *Enemy.LaserWeapon->GetName(), Enemy.LaserWeapon->Range / 100.f));
		}

		for (const FEnemyTierRole& Appearance : Enemy.Appearances)
		{
			const FSectorTierData* const TierData = UStartechTierSubsystem::FindTierData(Appearance.Tier);
			if (!TierData)
			{
				Table.Errors.Add(FString::Printf(TEXT("%s: tier %d is not in UStartechSettings::TierData."), *EnemyName, Appearance.Tier));
				continue;
			}

			const double EffectiveHealth = Enemy.Hull * TierData->EnemyHullMultiplier + Enemy.Shield * TierData->EnemyShieldMultiplier;
			const double EnemyDps = Enemy.DamagePerShot * TierData->EnemyDamageMultiplier * Enemy.LaserWeapon->FireRate * EnemyMounts;

			FLootRollModifiers Modifiers = FLootRollModifiers::FromTier(*TierData, ELootRollSource::EnemyDrop);
			Modifiers.CreditsMultiplier *= Enemy.RewardMultiplier;
			Modifiers.AmountMultiplier *= Enemy.RewardMultiplier;
			const double KillValue = Data->LootTable ? ExpectedLootValue(*Data->LootTable, Modifiers, Balance) : 0.0;

			bool bAnyCase = false;
			for (const FCombatGearCase& Case : Balance.GearCases)
			{
				if (Case.Tier != Appearance.Tier)
				{
					continue;
				}
				bAnyCase = true;

				UAmmoData* const Ammo = Settings.FindItemOfType<UAmmoData>(Case.AmmoTag);
				const float LaserScale = UpgradedValue(Catalog, Balance.LasersTrackId, Case.LasersLevel, StartechTags::Stat_Laser_Damage, 1.f);
				const double ShotDamage = PlayerLaser->Damage * UCombatBalance::GetDamageMultiplier(Ammo) * LaserScale;
				const double PlayerDps = ShotDamage * PlayerLaser->FireRate * PlayerMounts;
				const double PlayerHealth =
					UpgradedValue(Catalog, Balance.HullTrackId, Case.HullLevel, StartechTags::Stat_Hull_Max, ShipHealth->GetMaxHull())
					+ UpgradedValue(Catalog, Balance.ShieldTrackId, Case.ShieldLevel, StartechTags::Stat_Shield_Max, ShipHealth->GetMaxShield());
				if (ShotDamage <= 0.0 || PlayerHealth <= 0.0)
				{
					Table.Errors.Add(FString::Printf(TEXT("Tier %d %s: no player damage or health."), Case.Tier, *AmmoName(Ammo)));
					continue;
				}

				FCombatTTKRow& Row = Table.Rows.AddDefaulted_GetRef();
				Row.Tier = Appearance.Tier;
				Row.Enemy = EnemyName;
				Row.Role = Appearance.Role;
				Row.Gear = FString::Printf(TEXT("%s L%d"), *AmmoName(Ammo), Case.LasersLevel);
				Row.TimeToKill = static_cast<float>(EffectiveHealth / PlayerDps);
				Row.LoseOne = static_cast<float>(EnemyDps * Row.TimeToKill / PlayerHealth);
				Row.LoseThree = Row.LoseOne * ThreeAtOnceFactor;
				Row.Rounds = FMath::CeilToInt32(EffectiveHealth / ShotDamage - 1e-6);

				// Free ammo costs nothing; drop-only ammo was found, not bought.
				Row.AmmoCost = Row.Rounds * Balance.GetRoundCost(Ammo);
				Row.KillValue = KillValue;
				Row.CostShare = KillValue > 0.0 ? Row.AmmoCost / KillValue : (Row.AmmoCost > 0.0 ? 1.0 : 0.0);

				// Targets by role: regulars to the case, tough types up to ToughFactor × it, weak types none.
				const float Tolerance = Case.ToleranceSeconds;
				if (Row.Role == ECombatRole::Regular)
				{
					Row.AllowedMinTimeToKill = Case.MinTimeToKill - Tolerance;
					Row.AllowedMaxTimeToKill = Case.MaxTimeToKill + Tolerance;
					Row.AllowedMinLoss = Case.bCheckDamage ? Balance.OneFightLossMin : 0.f;
					Row.AllowedMaxLoss = Case.bCheckDamage ? Balance.OneFightLossMax : 0.f;
				}
				else if (Row.Role == ECombatRole::Tough)
				{
					Row.AllowedMaxTimeToKill = Case.MaxTimeToKill * Balance.ToughFactor + Tolerance;
					Row.AllowedMaxLoss = Case.bCheckDamage ? Balance.OneFightLossMax * Balance.ToughFactor : 0.f;
				}

				Row.bTimeToKillOutside = Row.AllowedMaxTimeToKill > 0.f
					&& (Row.TimeToKill < Row.AllowedMinTimeToKill - TimeEpsilon || Row.TimeToKill > Row.AllowedMaxTimeToKill + TimeEpsilon);
				Row.bLossOutside = Row.AllowedMaxLoss > 0.f
					&& (Row.LoseOne < Row.AllowedMinLoss - LossEpsilon || Row.LoseOne > Row.AllowedMaxLoss + LossEpsilon);
				Row.bCostOutside = Row.CostShare > Balance.MaxAmmoCostShare;
			}
			if (!bAnyCase)
			{
				Table.Errors.Add(FString::Printf(TEXT("%s appears in tier %d, which has no gear case."), *EnemyName, Appearance.Tier));
			}
		}
	}

	Table.Rows.StableSort([](const FCombatTTKRow& A, const FCombatTTKRow& B) { return A.Tier < B.Tier; });
	return Table;
}

TArray<FString> FCombatTTKTable::ToLines() const
{
	using namespace CombatTTK;

	static const TCHAR* const RoleNames[] = { TEXT("regular"), TEXT("tough"), TEXT("weak") };

	TArray<FString> Lines;
	Lines.Add(TEXT("Tier Enemy    Role     Gear      TTK      Lose1    Lose3  Rounds  Ammo cr  Kill cr  Cost%    Targets"));
	for (const FCombatTTKRow& Row : Rows)
	{
		FString Targets;
		if (Row.AllowedMaxTimeToKill > 0.f)
		{
			Targets += FString::Printf(TEXT("TTK %.2f-%.2f s"), Row.AllowedMinTimeToKill, Row.AllowedMaxTimeToKill);
		}
		if (Row.AllowedMaxLoss > 0.f)
		{
			Targets += FString::Printf(TEXT(", loss %.0f-%.0f%%"), Row.AllowedMinLoss * 100.f, Row.AllowedMaxLoss * 100.f);
		}
		Lines.Add(FString::Printf(TEXT("%-4d %-9s %-8s %-9s %-8s %-8s %5.0f%%  %6d  %7.1f  %7.1f  %-7s  %s"),
			Row.Tier, *Row.Enemy, RoleNames[static_cast<int32>(Row.Role)], *Row.Gear,
			*Mark(FString::Printf(TEXT("%.2fs"), Row.TimeToKill), Row.bTimeToKillOutside),
			*Mark(FString::Printf(TEXT("%.1f%%"), Row.LoseOne * 100.f), Row.bLossOutside),
			Row.LoseThree * 100.f, Row.Rounds, Row.AmmoCost, Row.KillValue,
			*Mark(FString::Printf(TEXT("%.1f%%"), Row.CostShare * 100.0), Row.bCostOutside),
			Targets.IsEmpty() ? TEXT("-") : *Targets));
	}
	Lines.Add(FString::Printf(TEXT("%d row(s), %d outside a target ('*'). Lose3 = three at once, killed in turn (no target: meant to be lethal)."),
		Rows.Num(), CountOutside()));
	for (const FString& Error : Errors)
	{
		Lines.Add(TEXT("ERROR: ") + Error);
	}
	for (const FString& Note : Notes)
	{
		Lines.Add(TEXT("Note: ") + Note);
	}
	return Lines;
}

int32 FCombatTTKTable::CountOutside() const
{
	return Rows.FilterByPredicate([](const FCombatTTKRow& Row) { return Row.IsOutside(); }).Num();
}
