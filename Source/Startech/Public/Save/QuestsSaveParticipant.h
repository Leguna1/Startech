#pragma once

#include "CoreMinimal.h"
#include "Save/StartechSaveTypes.h"
#include "SaveParticipant.h"
#include "UObject/Object.h"
#include "QuestsSaveParticipant.generated.h"

class UBountyBoardSubsystem;
class UQuestSubsystem;

/**
 * Startech.Quests: every quest's state (UQuestSubsystem::GetAllStates) and the bounty board's seed counter
 * (FStartechQuestsSave). The current bounty offers are quest states, so they come back unchanged.
 * Created and registered by UStartechSaveSubsystem when the game instance starts: quests are restored in the menu,
 * long before the hub can open.
 */
UCLASS()
class STARTECH_API UQuestsSaveParticipant : public UObject, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Quests". */
	static const FName ParticipantId;

	/** Remembers the quest subsystem and bounty board this participant saves, and follows their events. */
	void Init(UQuestSubsystem& InQuests, UBountyBoardSubsystem& InBoard);

	/**
	 * True while ReadSaveData or OnNewGame is restoring: RestoreStates fires OnQuestStatusChanged for every quest,
	 * and listeners (autosave on turn-in) must ignore those.
	 */
	bool IsRestoring() const { return bRestoring; }

	/** True when the active slot's quests began with OnNewGame (no saved quests), false after ReadSaveData. */
	bool StartedFresh() const { return bStartedFresh; }

	/**
	 * True when the bounty board starts empty and needs a first refresh once the player is placed: a new game, or an
	 * old save whose bounties were dropped by the version 2 migration.
	 */
	bool NeedsBoardRefresh() const { return bStartedFresh || bBountiesDropped; }

	/** "Startech.Quests". */
	virtual FName GetSaveParticipantId() const override;

	/** 3 (bounties from versions 1 and 2 are dropped). */
	virtual int32 GetSaveVersion() const override;

	/**
	 * Writes the quest states and the next bounty seed. If the quest subsystem has already shut down (it empties its
	 * quests in Deinitialize), writes the last data seen instead, so a save at shutdown never erases the quests.
	 */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Restores the quest states (UQuestSubsystem::RestoreStates) and the seed counter. */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Resets every quest to its initial state and starts the bounty seeds at a new random value. */
	virtual void OnNewGame() override;

private:
	/** Puts Save into the quest subsystem and the board, then remembers it. */
	void Apply(const FStartechQuestsSave& Save);

	/** Copies the live quest states and seed into LastSave, while the quest subsystem is running. */
	void RefreshLastSave() const;

	/** Quest event: keeps LastSave current. */
	UFUNCTION()
	void HandleQuestStatusChanged(FName QuestId, EQuestStatus NewStatus);

	/** Quest event: keeps LastSave current. */
	UFUNCTION()
	void HandleObjectiveProgress(FName QuestId, int32 ObjectiveIndex, int32 Current, int32 Required);

	/** Quest event: keeps LastSave current. */
	UFUNCTION()
	void HandleQuestTimeChanged(FName QuestId, float SecondsRemaining);

	/** Bounty board event: keeps LastSave current. */
	UFUNCTION()
	void HandleBoardRefreshed();

	/** The quest subsystem. */
	TWeakObjectPtr<UQuestSubsystem> Quests;

	/** The bounty board. */
	TWeakObjectPtr<UBountyBoardSubsystem> Board;

	/**
	 * The quests as last seen: refreshed on every quest and board event, every write and every restore. Written
	 * instead when the quest subsystem is gone (it can shut down before the shutdown save).
	 */
	mutable FStartechQuestsSave LastSave;

	/** See IsRestoring. */
	bool bRestoring = false;

	/** See StartedFresh. */
	bool bStartedFresh = false;

	/** The loaded save was older than version 3 and its generated bounties were dropped. See NeedsBoardRefresh. */
	bool bBountiesDropped = false;
};
