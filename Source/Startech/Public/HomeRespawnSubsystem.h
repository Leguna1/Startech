#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "HomeRespawnSubsystem.generated.h"

class AStartechShipPawn;
class APlayerController;
class USectorDirectorSubsystem;
class UWarpTransitionComponent;

/** Stage of the player's death-and-respawn sequence. */
UENUM()
enum class EHomeRespawnState : uint8
{
	/** No sequence: the player is alive. */
	Idle,
	/** The player died; the screen is fading to black and the ship's respawn timer is running. */
	Dying,
	/** The respawn is due, but a regeneration or a warp is still running; waiting for both to finish. */
	WaitingForDirector,
	/** The home sector was requested; waiting for its OnSectorReady. */
	Regenerating
};

/** Broadcast when the death-and-respawn sequence starts (true, at death) and ends (false, after the respawn). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRespawnSequenceChanged, bool, bActive);

/**
 * Brings a destroyed player back to the home sector's station (UStartechSettings::HomeSectorId).
 * The ship keeps the timing: on death it calls BeginDeathSequence, which covers the screen, and when its respawn
 * delay is over it calls RespawnPlayer. If the level is not the home sector, this regenerates it as home with
 * USectorDirectorSubsystem::RequestSector behind the black screen, waiting first for any regeneration or warp in
 * progress, then places the ship at the station's respawn point and fades back in.
 * A warp that ends during the sequence has its fade-in and input unlock undone, so the screen stays covered.
 */
UCLASS()
class STARTECH_API UHomeRespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Starts the sequence for a ship that has just died: remembers the death sector and the dropped cargo, fades to
	 * black and broadcasts OnRespawnSequenceChanged(true). bCargoDropped says whether death cargo was dropped, and
	 * CargoExpiryTime is the world time it expires (negative for never).
	 */
	void BeginDeathSequence(AStartechShipPawn& Ship, bool bCargoDropped, double CargoExpiryTime);

	/** Called by the ship when its respawn delay is over: respawns it at home, regenerating the level if needed. */
	void RespawnPlayer(AStartechShipPawn& Ship);

	/** True from the death until the ship has respawned. */
	bool IsRespawning() const { return State != EHomeRespawnState::Idle; }

	/** True while respawning when the player died away from home, or the level is being regenerated as home. */
	bool IsReturningHome() const;

	/** The home sector's name from the sector catalog, or its id when it has none. */
	FText GetHomeDisplayName() const;

	/** The name of the sector the player died in, where any death cargo stays. */
	FText GetDeathSectorDisplayName() const;

	/** True while respawning if the death dropped cargo. */
	bool HasDeathCargo() const { return IsRespawning() && bDeathCargo; }

	/** Seconds of world time until the death cargo expires (0 once expired), or a negative value if it never does. */
	float GetDeathCargoSecondsRemaining() const;

	/** Fires on BeginDeathSequence (true) and when the ship has respawned (false). The HUD folds and boots on it. */
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FOnRespawnSequenceChanged OnRespawnSequenceChanged;

	/** Clears the timers and unbinds from the director and the warp component. */
	virtual void Deinitialize() override;

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Director callback: retries the return home after a busy wait, or finishes once the home sector is ready. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Warp callback: a warp finished during the sequence, so the screen is covered again and input locked again. */
	UFUNCTION()
	void HandleWarpFinished(FName SectorId);

	/**
	 * Once no regeneration or warp is running: respawns at once in the home sector (or in a level without sectors),
	 * otherwise requests the home sector. While one is running, waits and polls.
	 */
	void TryReturnHome();

	/** Polling fallback while waiting, for a warp that went idle without broadcasting OnWarpFinished. */
	void HandleWaitPoll();

	/** The respawn took too long: logs an error and respawns at the world origin. */
	void HandleTimeout();

	/** Respawns the ship at Spawn, fades in, ends the sequence and broadcasts OnRespawnSequenceChanged(false). */
	void FinishRespawn(const FTransform& Spawn);

	/** The nearest in-play station's respawn point, else where the ship began play. */
	FTransform FindSpawnTransform(const AStartechShipPawn& Ship) const;

	/** Fades the player's camera from its current fade amount to ToAlpha over Seconds (0 for at once), holding it. */
	void FadeTo(float ToAlpha, float Seconds) const;

	/** Disables input on the ship, after a warp re-enabled it. */
	void LockShipInput() const;

	/** Binds to the director's OnSectorReady and the ship's warp component. */
	void BindEvents();

	/** Unbinds everything BindEvents bound and clears the timers. */
	void UnbindEvents();

	/** The world's sector director, or null in a level without ProcSector. */
	USectorDirectorSubsystem* GetDirector() const;

	/** The ship's player controller, or null. */
	APlayerController* GetShipController() const;

	/** The catalog name of SectorId, or the id itself; empty for none. */
	FText GetSectorDisplayName(FName SectorId) const;

	/** Current stage of the sequence. */
	EHomeRespawnState State = EHomeRespawnState::Idle;

	/** The ship being respawned. */
	TWeakObjectPtr<AStartechShipPawn> Ship;

	/** That ship's warp component (added in BP_Ship), if any. */
	TWeakObjectPtr<UWarpTransitionComponent> BoundWarp;

	/** The director bound to while the sequence runs. */
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;

	/** Sector the current sector was at the moment of death; NAME_None without ProcSector. */
	FName DeathSectorId;

	/** True once the home sector has been requested for this respawn. */
	bool bRegenerationRequested = false;

	/** True if the death dropped cargo. */
	bool bDeathCargo = false;

	/** World time the death cargo expires; negative for never. */
	double DeathCargoExpiryTime = -1.0;

	/** Polls while waiting for a regeneration or warp to finish. */
	FTimerHandle WaitTimer;

	/** Runs out RespawnTimeoutSeconds after the respawn was due. */
	FTimerHandle TimeoutTimer;
};
