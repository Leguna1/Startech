#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "StartechAutosaveSubsystem.generated.h"

class AStartechShipPawn;
class UHomeRespawnSubsystem;
class UQuestSubsystem;
class USectorDirectorSubsystem;
class UWarpTransitionComponent;

/**
 * Startech's autosave: turns game events into SaveFramework RequestSave calls (debounced there), with a reason.
 * There is no manual save.
 *   Docked / Undocked     the ship's OnDockedChanged
 *   QuestTurnedIn         a quest becomes Completed (not while the quests are being restored from a save)
 *   Trade                 a shop trade that moved at least one unit (FShopTrading::OnTradeCompleted), one per trade
 *   SectorReady           the director's OnSectorReady (after warps and respawns), and once when the game has started
 * Quitting saves with SaveNow("Quit") through UStartechGameFlowSubsystem instead.
 * Never mid-sequence: while the player is dying or respawning, a warp runs, a sector generates or the game is still
 * starting, a request is held (one, with the latest reason) and made when that sequence ends.
 * Active in the game map once UStartechStartFlowSubsystem reports the game started.
 */
UCLASS()
class STARTECH_API UStartechAutosaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Waits for the start flow's OnGameStarted. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Unbinds everything. */
	virtual void Deinitialize() override;

	/**
	 * Requests a save for Reason now, or holds it until the running death, respawn, warp, generation or start ends.
	 * Ignored before the game has started.
	 */
	void RequestAutosave(FName Reason);

	/** True while a death, respawn, warp, generation or the start is running: saves wait. */
	bool IsMidSequence() const;

	/** True while a request is held for the end of a sequence. */
	bool HasHeldRequest() const { return !HeldReason.IsNone(); }

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Start flow: binds to the ship, warp, director, respawn and quests, and saves the arrival. */
	UFUNCTION()
	void HandleGameStarted();

	/** FShopTrading::OnTradeCompleted: a trade in this world asks for a "Trade" save. */
	void HandleTradeCompleted(const struct FTradeOutcome& Outcome, AActor& Trader);

	/** The OnTradeCompleted binding. */
	FDelegateHandle TradeHandle;

	/** Ship: docked or undocked. */
	UFUNCTION()
	void HandleDockedChanged(bool bDocked, FText StationName);

	/** Quests: a quest reached Completed. */
	UFUNCTION()
	void HandleQuestStatusChanged(FName QuestId, EQuestStatus NewStatus);

	/** Director: a sector is ready. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Warp ended: makes a held request. */
	UFUNCTION()
	void HandleWarpFinished(FName SectorId);

	/** Death-and-respawn sequence ended: makes a held request. */
	UFUNCTION()
	void HandleRespawnSequenceChanged(bool bSequenceActive);

	/** Makes the held request if nothing blocks it any more. */
	void ReleaseHeldRequest();

	/** The game instance's quest subsystem, or null. */
	UQuestSubsystem* GetQuests() const;

	/** True once the game has started and the bindings are in place. */
	bool bActive = false;

	/** The held request's reason; None when nothing is held. */
	FName HeldReason;

	/** The player's ship. */
	TWeakObjectPtr<AStartechShipPawn> Ship;

	/** The ship's warp component. */
	TWeakObjectPtr<UWarpTransitionComponent> Warp;

	/** The world's director. */
	TWeakObjectPtr<USectorDirectorSubsystem> Director;

	/** The world's respawn subsystem. */
	TWeakObjectPtr<UHomeRespawnSubsystem> Respawn;
};
