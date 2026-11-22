#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StartechGameFlowSubsystem.generated.h"

/**
 * What the main menu (and later the pause menu) can do, kept out of the widgets: list the player's slots, start a new
 * game, continue, load, delete, quit. Starting or loading makes the slot active in SaveFramework (which restores the
 * game-instance participants at once) and opens UStartechSettings::GameMap, where UStartechStartFlowSubsystem takes
 * over. Only slots below UStartechSettings::VisibleSaveSlots are offered; the last slot is the editor's test slot.
 */
UCLASS()
class STARTECH_API UStartechGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of WorldContext's game instance, or null. */
	static UStartechGameFlowSubsystem* Get(const UObject* WorldContext);

	/** Needs the save subsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The slots the menu shows (0 to VisibleSaveSlots - 1), re-read from disk. */
	TArray<FSaveSlotInfo> GetVisibleSlotInfos() const;

	/** The visible slot saved most recently, or INDEX_NONE when none is occupied. */
	int32 FindMostRecentSlot() const;

	/** True when at least one visible slot is occupied. */
	bool HasAnySave() const;

	/**
	 * Starts a new game in SlotIndex (overwriting it) with Callsign, then opens the game map. Nothing is written until
	 * the first save. False with OutReason when the slot is not a visible one or the callsign breaks FCallsignRules.
	 */
	bool StartNewGame(int32 SlotIndex, const FString& Callsign, FText& OutReason);

	/** Loads SlotIndex and opens the game map. False with OutReason when it cannot be loaded. */
	bool LoadGame(int32 SlotIndex, FText& OutReason);

	/** Loads the most recently saved visible slot and opens the game map. False with OutReason when there is none. */
	bool ContinueGame(FText& OutReason);

	/** Deletes SlotIndex and its backup. False for a slot that is not visible. */
	bool DeleteSlot(int32 SlotIndex);

	/** Closes the game (from the main menu, where no slot is active). */
	void QuitGame();

	/**
	 * From play (the pause menu): saves the active slot at once (SaveNow "Quit"), closes it, unpauses and opens
	 * UStartechSettings::MainMenuMap.
	 */
	void QuitToMainMenu();

	/** From play (the pause menu): saves the active slot at once (SaveNow "Quit"), closes it and closes the game. */
	void QuitGameFromPlay();

private:
	/** True for a slot the menu offers (0 to VisibleSaveSlots - 1). */
	static bool IsVisibleSlot(int32 SlotIndex);

	/** Opens UStartechSettings::GameMap. */
	void OpenGameMap();

	/** Saves the active slot synchronously for quitting and closes it, so nothing saves it again on the way out. */
	void SaveAndCloseForQuit();
};
