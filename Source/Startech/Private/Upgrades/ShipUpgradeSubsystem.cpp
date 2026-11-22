#include "Upgrades/ShipUpgradeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechUpgrades, Log, All);

namespace ShipUpgrades
{
	/** Scaled credit costs are rounded to this. */
	constexpr float CreditRounding = 10.f;
}

UShipUpgradeSubsystem* UShipUpgradeSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UShipUpgradeSubsystem>() : nullptr;
}

const UUpgradeCatalog* UShipUpgradeSubsystem::GetCatalog() const
{
	if (!Catalog)
	{
		Catalog = UStartechSettings::Get().UpgradeCatalog.LoadSynchronous();
		if (!Catalog && !bReportedMissingCatalog)
		{
			bReportedMissingCatalog = true;
			UE_LOG(LogStartechUpgrades, Error, TEXT("UStartechSettings::UpgradeCatalog is not set or missing: no upgrades."));
		}
	}
	return Catalog;
}

const FUpgradeTrack* UShipUpgradeSubsystem::FindTrack(const FName TrackId) const
{
	const UUpgradeCatalog* const Loaded = GetCatalog();
	return Loaded ? Loaded->FindTrack(TrackId) : nullptr;
}

int32 UShipUpgradeSubsystem::GetLevel(const FName TrackId) const
{
	const FStartechUpgradeLevel* const Entry = Levels.FindByPredicate(
		[TrackId](const FStartechUpgradeLevel& Each) { return Each.TrackId == TrackId; });
	return Entry ? Entry->Level : 0;
}

int32 UShipUpgradeSubsystem::GetMaxLevel(const FName TrackId) const
{
	const FUpgradeTrack* const Track = FindTrack(TrackId);
	return Track ? Track->GetMaxLevel() : 0;
}

bool UShipUpgradeSubsystem::GetLevelCost(const FName TrackId, const int32 Level, FUpgradeLevelCost& OutCost) const
{
	const FUpgradeTrack* const Track = FindTrack(TrackId);
	if (!Track || !Track->LevelCosts.IsValidIndex(Level - 1))
	{
		return false;
	}

	OutCost = Track->LevelCosts[Level - 1];
	const float Scaled = OutCost.Credits * FMath::Max(UStartechSettings::Get().UpgradeCostScale, 0.f);
	OutCost.Credits = FMath::RoundToInt(Scaled / ShipUpgrades::CreditRounding) * static_cast<int32>(ShipUpgrades::CreditRounding);
	return true;
}

bool UShipUpgradeSubsystem::CanUpgrade(const FName TrackId, FText& OutReason) const
{
	const int32 Level = GetLevel(TrackId);
	FUpgradeLevelCost Cost;
	if (!FindTrack(TrackId))
	{
		OutReason = NSLOCTEXT("Upgrades", "UnknownTrack", "Unknown upgrade");
		return false;
	}
	if (!GetLevelCost(TrackId, Level + 1, Cost))
	{
		OutReason = NSLOCTEXT("Upgrades", "MaxLevel", "Max level");
		return false;
	}

	const AStartechShipPawn* const Ship = GetPlayerShip();
	const UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr;
	if (!Inventory || !Ship->IsDocked())
	{
		OutReason = NSLOCTEXT("Upgrades", "NotDocked", "Dock to upgrade");
		return false;
	}

	if (Inventory->GetCredits() < Cost.Credits)
	{
		OutReason = FText::Format(NSLOCTEXT("Upgrades", "NeedCredits", "Need {0} more credits"),
			FText::AsNumber(Cost.Credits - Inventory->GetCredits()));
		return false;
	}

	for (const FUpgradeMineralCost& Mineral : Cost.Minerals)
	{
		const UItemData* const Item = UStartechSettings::Get().FindItemOfType<UItemData>(Mineral.MineralTag);
		const int32 Held = Item ? Inventory->GetItemCount(Item) : 0;
		if (Held < Mineral.Count)
		{
			OutReason = FText::Format(NSLOCTEXT("Upgrades", "NeedMineral", "Need {0} more {1}"),
				FText::AsNumber(Mineral.Count - Held),
				Item ? Item->DisplayName : FText::FromString(Mineral.MineralTag.ToString()));
			return false;
		}
	}

	OutReason = FText::GetEmpty();
	return true;
}

bool UShipUpgradeSubsystem::Upgrade(const FName TrackId)
{
	FText Reason;
	if (!CanUpgrade(TrackId, Reason))
	{
		UE_LOG(LogStartechUpgrades, Log, TEXT("Upgrade %s refused: %s"), *TrackId.ToString(), *Reason.ToString());
		return false;
	}

	const int32 NewLevel = GetLevel(TrackId) + 1;
	FUpgradeLevelCost Cost;
	GetLevelCost(TrackId, NewLevel, Cost);

	// CanUpgrade checked every part, so each of these succeeds.
	UInventoryComponent* const Inventory = GetPlayerShip()->GetInventory();
	Inventory->SpendCredits(Cost.Credits);
	for (const FUpgradeMineralCost& Mineral : Cost.Minerals)
	{
		Inventory->RemoveItem(UStartechSettings::Get().FindItemOfType<UItemData>(Mineral.MineralTag), Mineral.Count);
	}

	StoreLevel(TrackId, NewLevel);
	UE_LOG(LogStartechUpgrades, Log, TEXT("Upgraded %s to level %d for %d credits."), *TrackId.ToString(), NewLevel,
		Cost.Credits);
	OnUpgradeChanged.Broadcast(TrackId, NewLevel);
	return true;
}

bool UShipUpgradeSubsystem::SetLevel(const FName TrackId, const int32 Level)
{
	const FUpgradeTrack* const Track = FindTrack(TrackId);
	if (!Track)
	{
		return false;
	}

	const int32 Clamped = FMath::Clamp(Level, 0, Track->GetMaxLevel());
	StoreLevel(TrackId, Clamped);
	UE_LOG(LogStartechUpgrades, Log, TEXT("%s set to level %d."), *TrackId.ToString(), Clamped);
	OnUpgradeChanged.Broadcast(TrackId, Clamped);
	return true;
}

float UShipUpgradeSubsystem::GetStatMultiplier(const FGameplayTag& StatTag) const
{
	return 1.f + SumBonuses(StatTag, EUpgradeBonusKind::Percent, NAME_None, 0);
}

float UShipUpgradeSubsystem::GetStatBonus(const FGameplayTag& StatTag) const
{
	return SumBonuses(StatTag, EUpgradeBonusKind::Flat, NAME_None, 0);
}

float UShipUpgradeSubsystem::GetStatMultiplierWith(const FGameplayTag& StatTag, const FName TrackId, const int32 Level) const
{
	return 1.f + SumBonuses(StatTag, EUpgradeBonusKind::Percent, TrackId, Level);
}

float UShipUpgradeSubsystem::GetStatBonusWith(const FGameplayTag& StatTag, const FName TrackId, const int32 Level) const
{
	return SumBonuses(StatTag, EUpgradeBonusKind::Flat, TrackId, Level);
}

void UShipUpgradeSubsystem::RestoreLevels(const TArray<FStartechUpgradeLevel>& NewLevels)
{
	Levels.Reset();
	for (const FStartechUpgradeLevel& Entry : NewLevels)
	{
		const FUpgradeTrack* const Track = FindTrack(Entry.TrackId);
		if (!Track)
		{
			UE_LOG(LogStartechUpgrades, Warning, TEXT("Saved upgrade track '%s' is not in the catalog; dropped."),
				*Entry.TrackId.ToString());
			continue;
		}
		StoreLevel(Entry.TrackId, FMath::Clamp(Entry.Level, 0, Track->GetMaxLevel()));
	}
	OnUpgradeChanged.Broadcast(NAME_None, 0);
}

void UShipUpgradeSubsystem::ResetLevels()
{
	Levels.Reset();
	OnUpgradeChanged.Broadcast(NAME_None, 0);
}

AStartechShipPawn* UShipUpgradeSubsystem::GetPlayerShip() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	const APlayerController* const Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	return Controller ? Cast<AStartechShipPawn>(Controller->GetPawn()) : nullptr;
}

float UShipUpgradeSubsystem::SumBonuses(const FGameplayTag& StatTag, const EUpgradeBonusKind Kind,
	const FName OverrideTrack, const int32 OverrideLevel) const
{
	const UUpgradeCatalog* const Loaded = GetCatalog();
	if (!Loaded)
	{
		return 0.f;
	}

	float Sum = 0.f;
	for (const FUpgradeTrack& Track : Loaded->Tracks)
	{
		const int32 Level = (!OverrideTrack.IsNone() && Track.TrackId == OverrideTrack) ? OverrideLevel : GetLevel(Track.TrackId);
		if (Level <= 0)
		{
			continue;
		}
		for (const FUpgradeStatEffect& Effect : Track.Effects)
		{
			if (Effect.Kind == Kind && Effect.StatTag == StatTag)
			{
				Sum += Effect.GetBonus(Level);
			}
		}
	}
	return Sum;
}

void UShipUpgradeSubsystem::StoreLevel(const FName TrackId, const int32 Level)
{
	const int32 Index = Levels.IndexOfByPredicate(
		[TrackId](const FStartechUpgradeLevel& Each) { return Each.TrackId == TrackId; });
	if (Level <= 0)
	{
		if (Index != INDEX_NONE)
		{
			Levels.RemoveAt(Index);
		}
		return;
	}

	if (Index != INDEX_NONE)
	{
		Levels[Index].Level = Level;
	}
	else
	{
		FStartechUpgradeLevel& Entry = Levels.AddDefaulted_GetRef();
		Entry.TrackId = TrackId;
		Entry.Level = Level;
	}
}
