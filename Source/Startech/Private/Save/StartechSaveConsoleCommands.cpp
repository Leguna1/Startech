#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "AmmoData.h"
#include "Bounties/BountyBoardSubsystem.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerProfileSubsystem.h"
#include "Runtime/QuestSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Upgrades/ShipUpgradeSubsystem.h"
#include "WeaponData.h"
#include "WeaponMountComponent.h"
#include "WeaponSystemComponent.h"

/**
 * Save debug commands (non-shipping):
 *   Startech.Save.Dump                 logs everything the Startech participants save, as it is now
 *   Startech.Save.Give <ItemTag> <N>   adds N of a catalog good: a mineral to the cargo, ammo, or a spare weapon
 */
namespace StartechSaveConsole
{
	/** The player's ship in World, or null. */
	static AStartechShipPawn* GetShip(const UWorld* const World)
	{
		return World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	}

	/** Good's tag for the log, or its asset name when untagged. */
	static FString Describe(const UTradeableData* const Good)
	{
		if (!Good)
		{
			return TEXT("none");
		}
		return Good->ItemTag.IsValid() ? Good->ItemTag.ToString() : FString::Printf(TEXT("%s (UNTAGGED)"), *Good->GetName());
	}

	static void Dump(const TArray<FString>& Args, UWorld* const World)
	{
		if (const UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(World))
		{
			UE_LOG(LogStartechSave, Display, TEXT("[Profile] callsign '%s', ship id %s"), *Profile->GetCallsign().ToString(),
				*Profile->GetShipId().ToString());
		}

		if (const AStartechShipPawn* const Ship = GetShip(World))
		{
			const UInventoryComponent* const Inventory = Ship->GetInventory();
			UE_LOG(LogStartechSave, Display, TEXT("[Wallet] %d credits"), Inventory->GetCredits());
			UE_LOG(LogStartechSave, Display, TEXT("[Cargo] %d / %d"), Inventory->GetCargoUsed(), Inventory->GetCargoCapacity());
			for (const FItemStack& Stack : Inventory->GetItems())
			{
				UE_LOG(LogStartechSave, Display, TEXT("    %s x%d"), *Describe(Stack.Item), Stack.Count);
			}

			TArray<UWeaponMountComponent*> Mounts;
			Ship->GetComponents(Mounts);
			for (const UWeaponMountComponent* const Mount : Mounts)
			{
				UE_LOG(LogStartechSave, Display, TEXT("[Arsenal] mount %s: %s"), *Mount->GetName(), *Describe(Mount->GetFittedWeapon()));
			}
			for (const FWeaponStack& Stack : Inventory->GetWeapons())
			{
				UE_LOG(LogStartechSave, Display, TEXT("[Arsenal] spare %s x%d"), *Describe(Stack.Weapon), Stack.Count);
			}
			const UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem();
			for (const FAmmoStack& Stack : Weapons->GetAmmoStacks())
			{
				UE_LOG(LogStartechSave, Display, TEXT("[Arsenal] ammo %s %.1f"), *Describe(Stack.Ammo), Stack.Count);
			}
			UE_LOG(LogStartechSave, Display, TEXT("[Arsenal] selected %s"), *Describe(Weapons->GetSelectedAmmo()));
		}
		else
		{
			UE_LOG(LogStartechSave, Display, TEXT("No player ship."));
		}

		const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
		if (const UQuestSubsystem* const Quests = GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr)
		{
			for (const FQuestRuntimeState& State : Quests->GetAllStates())
			{
				if (State.Status != EQuestStatus::Locked && (State.Status != EQuestStatus::Available || !State.SourceTemplateId.IsNone()
					|| State.TimesCompleted > 0))
				{
					UE_LOG(LogStartechSave, Display, TEXT("[Quests] %s %s progress [%s] time %.0f done x%d%s"),
						*State.QuestId.ToString(), *StaticEnum<EQuestStatus>()->GetNameStringByValue(static_cast<int64>(State.Status)),
						*FString::JoinBy(State.ObjectiveProgress, TEXT(","), [](const int32 Value) { return FString::FromInt(Value); }),
						State.TimeRemainingSeconds, State.TimesCompleted,
						State.SourceTemplateId.IsNone() ? TEXT("") : *FString::Printf(TEXT(" (bounty seed %d)"), State.GenerationSeed));
				}
			}
		}
		if (const UBountyBoardSubsystem* const Board = GameInstance ? GameInstance->GetSubsystem<UBountyBoardSubsystem>() : nullptr)
		{
			UE_LOG(LogStartechSave, Display, TEXT("[Quests] bounty board: %d offer(s), next seed %d"), Board->GetOffers().Num(),
				Board->GetNextSeed());
		}

		if (const UShipUpgradeSubsystem* const Upgrades = GameInstance ? GameInstance->GetSubsystem<UShipUpgradeSubsystem>() : nullptr)
		{
			UE_LOG(LogStartechSave, Display, TEXT("[Upgrades] %s"), Upgrades->GetLevels().Num() == 0 ? TEXT("every track at 0")
				: *FString::JoinBy(Upgrades->GetLevels(), TEXT(", "), [](const FStartechUpgradeLevel& Entry)
				{
					return FString::Printf(TEXT("%s %d"), *Entry.TrackId.ToString(), Entry.Level);
				}));
		}

		if (const USectorDirectorSubsystem* const Director = World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr)
		{
			for (const FSectorState& State : Director->GetStateStore().GetAllStates())
			{
				UE_LOG(LogStartechSave, Display, TEXT("[Sectors] %s: %d asteroid(s) removed, %d spawn(s) consumed"),
					*State.SectorId.ToString(), State.RemovedAsteroidIds.Num(), State.ConsumedSpawnIds.Num());
			}
		}
	}

	static void Give(const TArray<FString>& Args, UWorld* const World)
	{
		AStartechShipPawn* const Ship = GetShip(World);
		if (!Ship || Args.Num() < 2)
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Usage: Startech.Save.Give <ItemTag> <Count>, with a player ship."));
			return;
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), false);
		UTradeableData* const Good = UStartechSettings::Get().FindItem(Tag);
		const int32 Count = FCString::Atoi(*Args[1]);
		if (!Good || Count <= 0)
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Startech.Save.Give: '%s' is not in the item catalog, or the count is not positive."), *Args[0]);
			return;
		}

		if (UItemData* const Item = Cast<UItemData>(Good))
		{
			Ship->GetInventory()->AddItem(Item, Count);
		}
		else if (UAmmoData* const Ammo = Cast<UAmmoData>(Good))
		{
			Ship->GetWeaponSystem()->AddAmmo(Ammo, static_cast<float>(Count));
		}
		else if (UWeaponData* const Weapon = Cast<UWeaponData>(Good))
		{
			Ship->GetInventory()->AddWeapon(Weapon, Count);
		}
		UE_LOG(LogStartechSave, Display, TEXT("Startech.Save.Give: %d x %s"), Count, *Tag.ToString());
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(TEXT("Startech.Save.Dump"),
		TEXT("Logs everything Startech's save participants store, as it is now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Dump));

	static FAutoConsoleCommandWithWorldAndArgs GiveCommand(TEXT("Startech.Save.Give"),
		TEXT("Startech.Save.Give <ItemTag> <Count>: adds a catalog good (mineral to cargo, ammo, or spare weapon)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Give));
}

#endif
