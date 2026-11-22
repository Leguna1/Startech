#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Boost/ShipBoostSettings.h"
#include "Combat/ChargedShotTypes.h"
#include "Death/ShipDeathTypes.h"
#include "GameplayTagContainer.h"
#include "Save/StartechSaveTypes.h"
#include "Tiers/SectorTierTypes.h"
#include "StartechSettings.generated.h"

class UTradeableData;

/**
 * Startech's project-wide game settings (Project Settings > Game > Startech), saved in DefaultGame.ini.
 * Holds the home sector, the timing of the death-and-respawn sequence run by UHomeRespawnSubsystem, the item catalog
 * the new-game defaults and the sector tiers.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Startech"))
class STARTECH_API UStartechSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Places these settings under the Game category. */
	UStartechSettings();

	/** The settings' class default object. */
	static const UStartechSettings& Get();

	/**
	 * The ProcSector sector id of the home sector. The only sector with a station, and where the player respawns
	 * after dying anywhere; dying elsewhere regenerates the level as this sector first.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Respawn")
	FName HomeSectorId = TEXT("Terra");

	/** Seconds the screen takes to fade to black after the player's death. */
	UPROPERTY(Config, EditAnywhere, Category = "Respawn", meta = (ClampMin = "0.05", Units = "s"))
	float DeathFadeOutSeconds = 1.5f;

	/** Seconds the screen takes to fade back in once the player is at the home station. */
	UPROPERTY(Config, EditAnywhere, Category = "Respawn", meta = (ClampMin = "0.05", Units = "s"))
	float RespawnFadeInSeconds = 1.f;

	/**
	 * Longest wait, from the moment the respawn is due, for the home sector to be ready. When it runs out an error
	 * is logged and the player respawns at the world origin as a last resort.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Respawn", meta = (ClampMin = "1", Units = "s"))
	float RespawnTimeoutSeconds = 10.f;

	/**
	 * Seconds after the player last took damage during which they count as in combat. Warping is refused in combat;
	 * a gate retries the warp by itself once this has passed without another hit.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0", Units = "s"))
	float CombatCooldownSeconds = 5.f;

	/**
	 * The dying state every ship goes through between hull 0 and removal (UShipDeathComponent): its length, motion,
	 * flash, and the default effect hooks.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	FShipDeathSettings Death;

	/** The ranged enemy's charged shot: telegraph glow and the effect and sound hooks (its numbers are in DA_CombatBalance). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	FChargedShotEffects ChargedShot;

	/** Every combat balance number: DA_CombatBalance (UCombatBalance::Get). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	TSoftObjectPtr<class UCombatBalance> CombatBalance;

	/**
	 * Every good the game can name by its ItemTag: minerals, ammo and weapons. The single tag-to-asset lookup, used by
	 * quests (rewards, Collect objectives, item names) and by saves. Each asset's ItemTag must be unique.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Items")
	TArray<TSoftObjectPtr<UTradeableData>> ItemCatalog;

	/** The good in ItemCatalog whose ItemTag is exactly Tag, loaded, or null. */
	UTradeableData* FindItem(const FGameplayTag& Tag) const;

	/** The good in ItemCatalog whose ItemTag is exactly Tag, if it is a T, or null. */
	template <typename T>
	T* FindItemOfType(const FGameplayTag& Tag) const
	{
		return Cast<T>(FindItem(Tag));
	}

	/** Credits a new game starts with. */
	UPROPERTY(Config, EditAnywhere, Category = "New Game", meta = (ClampMin = "0"))
	int32 NewGameCredits = 0;

	/** Ammo a new game starts with, by ammo tag. */
	UPROPERTY(Config, EditAnywhere, Category = "New Game")
	TArray<FStartechTaggedAmount> NewGameAmmo;

	/** The weapon on each mount of the ship in a new game. New games have no spare weapons. */
	UPROPERTY(Config, EditAnywhere, Category = "New Game")
	TArray<FStartechMountWeapon> NewGameMountWeapons;

	/** The ammo selected at the start of a new game. */
	UPROPERTY(Config, EditAnywhere, Category = "New Game", meta = (Categories = "Item.Ammo"))
	FGameplayTag NewGameSelectedAmmo;

	/**
	 * How many save slots the main menu shows (slots 0 to VisibleSaveSlots - 1), and the only ones Continue picks from.
	 * SaveFramework's NumSlots is one more: the last slot is the editor's [PIE] test slot.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Saving", meta = (ClampMin = "1"))
	int32 VisibleSaveSlots = 3;

	/** The game map, opened by New game, Continue and Load. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> GameMap;

	/** The main menu map (L_MainMenu), opened by Quit to main menu. Its game mode is AStartechMenuGameMode. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> MainMenuMap;

	/** The main menu widget AStartechMenuGameMode shows: WBP_MainMenu. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftClassPtr<class UMainMenuWidget> MainMenuWidgetClass;

	/** The pause menu UStartechPauseMenuSubsystem shows in flight: WBP_PauseMenu. */
	UPROPERTY(Config, EditAnywhere, Category = "Pause")
	TSoftClassPtr<class UPauseMenuWidget> PauseMenuWidgetClass;

	/** The input action that opens the pause menu in flight (IA_Pause, mapped to Escape in IMC_Ship). */
	UPROPERTY(Config, EditAnywhere, Category = "Pause")
	TSoftObjectPtr<class UInputAction> PauseInputAction;

	/** The input action held to boost (IA_Boost, mapped to Left Shift in IMC_Ship). */
	UPROPERTY(Config, EditAnywhere, Category = "Boost")
	TSoftObjectPtr<class UInputAction> BoostInputAction;

	/** The boost's energy, speed and camera numbers (UShipBoostComponent). */
	UPROPERTY(Config, EditAnywhere, Category = "Boost")
	FShipBoostSettings Boost;

	/** Every upgrade track: DA_UpgradeCatalog (UShipUpgradeSubsystem). */
	UPROPERTY(Config, EditAnywhere, Category = "Upgrades")
	TSoftObjectPtr<class UUpgradeCatalog> UpgradeCatalog;

	/** Multiplies every upgrade's credit cost (rounded to 10). Mineral costs are never scaled. */
	UPROPERTY(Config, EditAnywhere, Category = "Upgrades", meta = (ClampMin = "0"))
	float UpgradeCostScale = 1.f;

	/** The action bar's slots and their keys: DA_ActionBarLayout (UActionBarComponent). */
	UPROPERTY(Config, EditAnywhere, Category = "HUD")
	TSoftObjectPtr<class UActionBarLayout> ActionBarLayout;

	/**
	 * Each sector's tier (Terra 1, Rust 2, Glacier 3, Jove 4). A sector missing here counts as tier 1. Read it only
	 * through UStartechTierSubsystem.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Tiers")
	TArray<FSectorTierAssignment> SectorTiers;

	/** One entry per tier: entry 0 is tier 1. Read it only through UStartechTierSubsystem. */
	UPROPERTY(Config, EditAnywhere, Category = "Tiers")
	TArray<FSectorTierData> TierData;
};
