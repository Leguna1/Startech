#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.h"
#include "StartechSaveTypes.generated.h"

/*
 * Every struct Startech writes into a save. All saved fields carry the SaveGame flag: SaveFramework serializes with
 * ArIsSaveGame, which skips anything else. Goods are stored by their ItemTag (UTradeableData::ItemTag), resolved
 * through UStartechSettings::ItemCatalog, never by object pointer. Some structs double as config entries in
 * UStartechSettings, hence the EditAnywhere.
 */

/** A whole number of one good, by tag: a cargo stack or spare weapons. */
USTRUCT(BlueprintType)
struct STARTECH_API FStartechTaggedCount
{
	GENERATED_BODY()

	/** The good's ItemTag (Item.Mineral.*, Item.Weapon.*). */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Save", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	/** How many. */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0"))
	int32 Count = 0;
};

/** An amount of one ammo type, by tag. Fractional, because beams use ammo continuously. */
USTRUCT(BlueprintType)
struct STARTECH_API FStartechTaggedAmount
{
	GENERATED_BODY()

	/** The ammo's ItemTag (Item.Ammo.*). */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Save", meta = (Categories = "Item.Ammo"))
	FGameplayTag ItemTag;

	/** Rounds held. */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "0"))
	float Amount = 0.f;
};

/** The weapon fitted to one mount of the player's ship. */
USTRUCT(BlueprintType)
struct STARTECH_API FStartechMountWeapon
{
	GENERATED_BODY()

	/** The mount component's name on the ship: LaserMountLeft, LaserMountRight or ShieldDrainMount. */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Save")
	FName MountName;

	/** The fitted weapon's ItemTag (Item.Weapon.*); empty for an empty mount. */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly, Category = "Save", meta = (Categories = "Item.Weapon"))
	FGameplayTag WeaponTag;
};

/** Startech.Profile: the pilot and the highest tier visited. The ship id is derived from the callsign and is not stored. */
USTRUCT()
struct STARTECH_API FStartechProfileSave
{
	GENERATED_BODY()

	/** The callsign as entered. */
	UPROPERTY(SaveGame)
	FString Callsign;

	/** The highest sector tier entered (version 2; version 1 saves load as 1). */
	UPROPERTY(SaveGame)
	int32 HighestTierVisited = 1;
};

/** Startech.Wallet: the credits. */
USTRUCT()
struct STARTECH_API FStartechWalletSave
{
	GENERATED_BODY()

	/** Credits held. */
	UPROPERTY(SaveGame)
	int32 Credits = 0;
};

/** Startech.Cargo: the cargo hold's contents. The capacity comes from the ship and is not stored. */
USTRUCT()
struct STARTECH_API FStartechCargoSave
{
	GENERATED_BODY()

	/** One entry per mineral held. */
	UPROPERTY(SaveGame)
	TArray<FStartechTaggedCount> Items;
};

/** Startech.Arsenal: weapons (fitted and spare), ammo and the selected ammo. */
USTRUCT()
struct STARTECH_API FStartechArsenalSave
{
	GENERATED_BODY()

	/** What each mount carries. Mounts not listed keep their current weapon. */
	UPROPERTY(SaveGame)
	TArray<FStartechMountWeapon> Mounts;

	/** Spare weapons in the inventory, not fitted to a mount. */
	UPROPERTY(SaveGame)
	TArray<FStartechTaggedCount> SpareWeapons;

	/** Every ammo type held. Types not listed are held at 0. */
	UPROPERTY(SaveGame)
	TArray<FStartechTaggedAmount> Ammo;

	/** The active weapon group's selected ammo (what the trigger fires); empty when nothing is selected. */
	UPROPERTY(SaveGame)
	FGameplayTag SelectedAmmo;

	/**
	 * Version 2: every weapon group's selected ammo, one tag per group. Missing in version 1 saves, where SelectedAmmo
	 * alone restores its own group and the others start on their first held ammo.
	 */
	UPROPERTY(SaveGame)
	TArray<FGameplayTag> GroupSelections;
};

/** Startech.Quests: every quest's state and the bounty board's seed counter. The offers are quest states. */
USTRUCT()
struct STARTECH_API FStartechQuestsSave
{
	GENERATED_BODY()

	/** UQuestSubsystem::GetAllStates. */
	UPROPERTY(SaveGame)
	TArray<FQuestRuntimeState> States;

	/** UBountyBoardSubsystem::GetNextSeed. */
	UPROPERTY(SaveGame)
	int32 NextBountySeed = 0;
};

/** One sector's remembered changes: a SaveGame copy of ProcSector's FSectorState. */
USTRUCT()
struct STARTECH_API FStartechSectorSave
{
	GENERATED_BODY()

	/** The sector. */
	UPROPERTY(SaveGame)
	FName SectorId;

	/** Asteroids mined or destroyed. */
	UPROPERTY(SaveGame)
	TArray<int32> RemovedAsteroidIds;

	/** Remembered spawns consumed (entries with Remember When Consumed; enemies respawn and are never here). */
	UPROPERTY(SaveGame)
	TArray<int32> ConsumedSpawnIds;
};

/** Startech.Sectors: ProcSector's FSectorStateStore. */
USTRUCT()
struct STARTECH_API FStartechSectorsSave
{
	GENERATED_BODY()

	/** One entry per sector with changes. */
	UPROPERTY(SaveGame)
	TArray<FStartechSectorSave> Sectors;
};

/** One upgrade track's level: in Startech.Upgrades, and UShipUpgradeSubsystem's runtime record. */
USTRUCT(BlueprintType)
struct STARTECH_API FStartechUpgradeLevel
{
	GENERATED_BODY()

	/** FUpgradeTrack::TrackId. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FName TrackId;

	/** 1 to the track's max level; tracks at 0 aren't listed. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Level = 0;
};

/** Startech.Upgrades: every upgrade track above level 0. */
USTRUCT()
struct STARTECH_API FStartechUpgradesSave
{
	GENERATED_BODY()

	/** The levels, by track id. */
	UPROPERTY(SaveGame)
	TArray<FStartechUpgradeLevel> Tracks;
};
