#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "SaveTypes.h"
#include "SaveSubsystem.generated.h"

class ISaveParticipant;

/** Fired when a save begins collecting records. Reason is what RequestSave or SaveNow was given. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveStarted, FName, Reason);
/** Fired when a save finished writing (or failed). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveCompleted, bool, bSuccess, FName, Reason);
/** Fired when a slot becomes the active slot, from LoadSlot or CreateNewGame. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotLoaded, int32, SlotIndex);
/** Fired when the slot list a menu shows has changed (after a save or delete). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSlotsChanged);
/** Fired once per active slot, when every id in ExpectedParticipantIds has registered and received its data. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllParticipantsLoaded);

/**
 * The facade of the framework: slots, participants, saving, play time.
 *
 * Flow: a menu calls CreateNewGame or LoadSlot (before or after the game world exists). Systems register as
 * participants when they are ready and immediately receive their record (ReadSaveData) or OnNewGame. RequestSave
 * collects every registered participant's record and writes the slot asynchronously, after a short debounce.
 *
 * Records are applied in registration order. Participants must not depend on each other's load order; a participant
 * that has to waits for OnAllParticipantsLoaded.
 */
UCLASS()
class SAVEFRAMEWORK_API USaveSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** The save subsystem of WorldContext's game instance, or null outside a running game. */
	static USaveSubsystem* Get(const UObject* WorldContext);

	// ---- USubsystem ----

	/** Resets the runtime state. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Saves the active slot when bSaveOnShutdown is set, then stops the debounce timer. */
	virtual void Deinitialize() override;

	// ---- FTickableGameObject (play time only) ----

	/** Adds the world's delta time to the active slot's play time. Not called while paused. */
	virtual void Tick(float DeltaTime) override;

	/** Stat id for the tickable. */
	virtual TStatId GetStatId() const override;

	/** Never for the class default object, conditional (IsTickable) otherwise. */
	virtual ETickableTickType GetTickableTickType() const override;

	/** True while a slot is active. */
	virtual bool IsTickable() const override;

	/** The game instance's world, so play time follows that world's pause and time dilation. */
	virtual UWorld* GetTickableGameObjectWorld() const override;

	// ---- Slots ----

	/** One entry per slot (NumSlots), read from disk the first time and cached; refreshed after each save or delete. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	TArray<FSaveSlotInfo> GetSlotInfos() const;

	/** Re-reads every slot header from disk and fires OnSlotsChanged. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void RefreshSlotInfos();

	/**
	 * Makes SlotIndex the active slot with no records. Every participant gets OnNewGame (registered ones now, later
	 * ones on registration). Nothing is written until the first save. False when SlotIndex is out of range.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool CreateNewGame(int32 SlotIndex, const FString& DisplayName);

	/**
	 * Reads SlotIndex (falling back to its backup) and makes it the active slot. Registered participants receive their
	 * record now, later ones on registration. False with OutReason when the slot is empty, out of range or unusable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadSlot(int32 SlotIndex, FText& OutReason);

	/** Deletes SlotIndex and its backup. Closes the slot first when it is the active one. True when it is gone. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool DeleteSlot(int32 SlotIndex);

	/** The active slot's index, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Save")
	int32 GetActiveSlot() const;

	/** True while a slot is active (after CreateNewGame or LoadSlot, until CloseActiveSlot). */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool HasActiveSlot() const;

	/** Forgets the active slot and its records without saving (for returning to a menu; save first if needed). */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void CloseActiveSlot();

	/** The active slot's header, with the current play time. Default when no slot is active. */
	UFUNCTION(BlueprintPure, Category = "Save")
	FSaveSlotHeader GetActiveHeader() const;

	/** Changes the name the active slot shows in menus from its next save on. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetActiveDisplayName(const FString& DisplayName);

	/** True when the active slot was read from its backup because the main file was corrupt. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool WasActiveSlotRestoredFromBackup() const;

	/** The file name of SlotIndex (SlotNamePrefix + index). */
	UFUNCTION(BlueprintPure, Category = "Save")
	FString GetSlotName(int32 SlotIndex) const;

	// ---- Participants ----

	/**
	 * Registers Participant (a UObject implementing ISaveParticipant), held weakly. Idempotent. While a slot is
	 * active it immediately receives its record (ReadSaveData) or OnNewGame. Refused when another registered
	 * object already uses the same participant id.
	 */
	void RegisterParticipant(UObject* Participant);

	/** Unregisters Participant. While a slot is active its current data is kept and written with the next save. */
	void UnregisterParticipant(UObject* Participant);

	/** True when Participant is registered. */
	bool IsParticipantRegistered(const UObject* Participant) const;

	/** Ids of the registered participants, in registration order. */
	TArray<FName> GetRegisteredParticipantIds() const;

	/** Ids of the active slot's records whose participant is not registered: their data waits for registration. */
	TArray<FName> GetPendingParticipantIds() const;

	// ---- Saving ----

	/**
	 * Asks for a save of the active slot. Requests within AutosaveDebounceSeconds of the first are merged; the save
	 * then runs asynchronously. A request while a save is writing queues exactly one more save. Ignored with no slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void RequestSave(FName Reason);

	/** Saves the active slot synchronously (for shutdown or quitting), after any save in flight. True on success. */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveNow(FName Reason);

	/** True while an asynchronous save is writing. */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsSaveInFlight() const;

	// ---- Events ----

	/** A save started collecting records. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnSaveStarted OnSaveStarted;

	/** A save finished writing, successfully or not. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnSaveCompleted OnSaveCompleted;

	/** A slot became the active slot (LoadSlot or CreateNewGame). */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnSlotLoaded OnSlotLoaded;

	/** The slot list changed (after a save, a delete or RefreshSlotInfos). */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnSlotsChanged OnSlotsChanged;

	/** Every id in ExpectedParticipantIds has registered and received its data for the active slot. Fires once per slot. */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FOnAllParticipantsLoaded OnAllParticipantsLoaded;

private:
	/** A registered participant: the object, held weakly, and its id at registration. */
	struct FRegisteredParticipant
	{
		/** The participant object. */
		TWeakObjectPtr<UObject> Object;

		/** Its GetSaveParticipantId when it registered. */
		FName Id;
	};

	/** Makes SlotIndex active with Header and Records, and gives every registered participant its data. */
	void ActivateSlot(int32 SlotIndex, const FSaveSlotHeader& Header, const TArray<FSaveRecord>& Records, bool bRestoredFromBackup);

	/** Gives Participant its record from KnownRecords, or OnNewGame when there is none, and checks the expected ids. */
	void ApplyToParticipant(ISaveParticipant& Participant, FName Id);

	/** Fires OnAllParticipantsLoaded once all ExpectedParticipantIds have received their data. */
	void CheckAllParticipantsLoaded();

	/** Replaces Record's entry in KnownRecords (by id), or adds it. */
	void StoreKnownRecord(FSaveRecord&& Record);

	/** Refreshes KnownRecords from every registered participant and drops participants that no longer exist. */
	void CollectRecords();

	/** The header to write now: active slot, display name, play time, current time and versions. */
	FSaveSlotHeader MakeHeaderForSave() const;

	/** Collects records and starts an asynchronous save of the active slot. */
	void StartAsyncSave(FName Reason);

	/** Ends an asynchronous save: updates the slot list, fires OnSaveCompleted, starts the queued save. */
	void HandleAsyncSaveCompleted(bool bSuccess, const FSaveSlotHeader& SavedHeader, FName Reason);

	/** Debounce timer callback: starts the save, or queues it when one is in flight. Returns false (one-shot). */
	bool HandleDebounceElapsed(float DeltaTime);

	/** Stops the debounce timer, dropping a requested save that has not started. */
	void CancelPendingSave();

	/** Blocks (pumping game-thread tasks, up to a few seconds) until the asynchronous save in flight completes. */
	void WaitForSaveInFlight();

	/** Marks SlotIndex as occupied with Header in the cache and fires OnSlotsChanged. */
	void UpdateSlotInfo(const FSaveSlotHeader& Header);

	/** Reads SlotIndex's header from disk (or its backup). */
	FSaveSlotInfo ReadSlotInfo(int32 SlotIndex) const;

	/** Fills SlotInfos from disk when it has not been read yet. */
	void EnsureSlotInfos() const;

	/** Index of Object in Participants, or INDEX_NONE. */
	int32 FindParticipantIndex(const UObject* Object) const;

	/** The active slot, or INDEX_NONE. */
	int32 ActiveSlotIndex = INDEX_NONE;

	/** The active slot's header; PlayTimeSeconds grows while playing. */
	FSaveSlotHeader ActiveHeader;

	/** True when the active slot came from its backup. */
	bool bActiveRestoredFromBackup = false;

	/** The latest known record of every participant of the active slot, registered or not. Written by each save. */
	TArray<FSaveRecord> KnownRecords;

	/** Registered participants, in registration order. */
	TArray<FRegisteredParticipant> Participants;

	/** Ids that received their data (record or OnNewGame) for the active slot. */
	TArray<FName> LoadedParticipantIds;

	/** True once OnAllParticipantsLoaded fired for the active slot. */
	bool bAllParticipantsLoadedFired = false;

	/** Cached slot list (GetSlotInfos). */
	mutable TArray<FSaveSlotInfo> SlotInfos;

	/** True once SlotInfos has been read from disk. */
	mutable bool bSlotInfosRead = false;

	/** The debounce timer of a requested save that has not started yet. */
	FTSTicker::FDelegateHandle PendingSaveHandle;

	/** Reason of the latest request merged into the pending save. */
	FName PendingSaveReason;

	/** True while an asynchronous save is writing. */
	bool bSaveInFlight = false;

	/** True when a save was requested while one was writing; it starts when that one completes. */
	bool bSaveQueued = false;

	/** Reason of the queued save. */
	FName QueuedSaveReason;
};
