#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "StartechStartFlowSubsystem.generated.h"

class AStartechShipPawn;
class USaveSubsystem;
class USectorDirectorSubsystem;

/** Broadcast once the game map has started: the home sector is ready and the player is docked at its station. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStarted);

/**
 * Starts the game map. ProcSector's automatic generation is off (bAutoGenerateOnBeginPlay=False): the screen stays
 * black and the ship's input locked until every save participant has its data (SaveFramework's
 * OnAllParticipantsLoaded), then the home sector is requested. When it is ready the ship is placed at the station's
 * respawn point, docked (without the dock's usual bounty board refresh, so the saved offers stay), and the screen
 * fades in. A game map started with no active slot (Play In Editor, or the map launched directly) first starts a
 * temporary new game in the test slot (UStartechSaveSubsystem::StartTestGame). Off in the main menu's world.
 */
UCLASS()
class STARTECH_API UStartechStartFlowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Binds to the save subsystem and the director, and starts the test game when no slot is active. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Unbinds and clears the retry timer. */
	virtual void Deinitialize() override;

	/** True from the start of the game map until the player is placed at the station (no saves in between). */
	bool IsStarting() const { return bActive && !bStarted; }

	/** True once the player has been placed and the screen faded in. */
	bool HasStarted() const { return bStarted; }

	/** Fires once, when the player is docked at the home station and the screen fades in. */
	UPROPERTY(BlueprintAssignable, Category = "Startech")
	FOnGameStarted OnGameStarted;

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Save callback: every participant has its data; covers the screen and requests the home sector. */
	UFUNCTION()
	void HandleAllParticipantsLoaded();

	/** Director callback: the home sector is ready; places the ship docked and fades in. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Requests the home sector, retrying on a timer while the director is busy. */
	void RequestHomeSector();

	/** Moves the ship to the home station's respawn point, docked, without refreshing the bounty board. */
	void PlaceShipAtStation(AStartechShipPawn& Ship) const;

	/**
	 * Hands the ship to the player on arrival: makes sure the controller possesses it, unlocks its input, and sets
	 * game-only input (FStartechInputState::SetGameOnly) whatever the previous screen left behind.
	 */
	void ReleasePlayer() const;

	/** Covers (1) or uncovers (0) the screen over Seconds; 0 is immediate. */
	void FadeTo(float ToAlpha, float Seconds) const;

	/** Locks or unlocks the ship's flight input. */
	void SetShipInputLocked(bool bLocked) const;

	/** The player's ship, or null. */
	AStartechShipPawn* GetShip() const;

	/** The game instance's save subsystem, or null. */
	USaveSubsystem* GetSaves() const;

	/** The world's director, or null. */
	USectorDirectorSubsystem* GetDirector() const;

	/** True in the game map (not the main menu). */
	bool bActive = false;

	/** True once the home sector was requested. */
	bool bSectorRequested = false;

	/** True once the player was placed. */
	bool bStarted = false;

	/** Retries RequestSector while the director is busy. */
	FTimerHandle RetryTimer;

	/** Runs one test-aid command (Startech.Flow.OnStartCommands, non-shipping), then waits for the next. */
	void RunNextStartCommand();

	/** Test-aid commands still to run. */
	TArray<FString> PendingStartCommands;

	/** Spaces the test-aid commands out (real time, so it runs while paused). */
	FTSTicker::FDelegateHandle StartCommandTicker;
};
