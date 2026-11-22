#pragma once

#include "CoreMinimal.h"
#include "QuestBridge.h"
#include "QuestTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "UObject/WeakInterfacePtr.h"
#include "QuestSubsystem.generated.h"

class UQuestDefinition;

/** Broadcast when a quest's status changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestStatusChanged, FName, QuestId, EQuestStatus, NewStatus);

/** Broadcast when an objective's count changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnQuestObjectiveProgress, FName, QuestId, int32, ObjectiveIndex, int32, Current, int32, Required);

/** Broadcast once per whole second while a timed quest's clock runs, and when it is accepted. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestTimeChanged, FName, QuestId, float, SecondsRemaining);

/** Broadcast when a quest leaves the registry (a generated bounty turned in, abandoned or dropped from the board). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUnregistered, FName, QuestId);

/**
 * The quest runtime (Facade): the registry of quest definitions, every quest's FQuestRuntimeState, and the rules that
 * move quests between statuses. The game reports tagged events with ReportEvent and supplies inventory, rewards and
 * the turn-in rule through the bridge setters; it listens to the On... events to update its UI.
 * Every event is broadcast after the call that caused it has finished changing state, so listeners may call back in.
 */
UCLASS()
class QUESTFRAMEWORK_API UQuestSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Loads every UQuestDefinition under the settings' QuestDefinitionPaths and sets each quest Locked or Available. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Drops every definition, state and bridge reference. */
	virtual void Deinitialize() override;

	// ---- Registry ----

	/** Adds a definition at runtime (for example a generated bounty). Fails if it has no id or the id is taken. */
	bool RegisterDefinition(UQuestDefinition* Definition);

	/** Removes a definition and its state, and broadcasts OnQuestUnregistered. False if the id is unknown. */
	bool UnregisterDefinition(FName QuestId);

	// ---- Game bridge ----

	/** The game's inventory, held weakly. Null clears it. Collect objectives read 0 without one. */
	void SetInventoryProvider(IQuestInventoryProvider* Provider);

	/** The game's reward granter, held weakly. Null clears it. Without one, rewards are only logged. */
	void SetRewardGranter(IQuestRewardGranter* Granter);

	/** The turn-in rule. Bind it with BindUObject or BindWeakLambda so it is weak. Unbound allows every turn-in. */
	void SetTurnInGate(const FQuestTurnInGate& Gate);

	// ---- Quest API ----

	/**
	 * Whether Accept would succeed now, without changing anything. False with a reason when the quest is unknown,
	 * locked, already active or completed, cooling down, or its category is full ("Limit reached: 3 bounties").
	 * Accept runs the same checks, so a UI that shows this reason never disagrees with the action.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Quest")
	bool CanAccept(FName QuestId, FText& OutReason) const;

	/**
	 * Whether TurnIn would succeed now, without changing anything. False with a reason when the quest is unknown or
	 * not ready, the turn-in gate refuses, its inventory objectives are no longer complete (read from the inventory),
	 * or items must be taken and no inventory is registered. TurnIn runs the same checks.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Quest")
	bool CanTurnIn(FName QuestId, FText& OutReason) const;

	/** Starts a quest. Fails with the reason from CanAccept when it can't be accepted. */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool Accept(FName QuestId, FText& OutReason);

	/** Gives up an active quest: it becomes Available again (a generated bounty is removed). False if it was not active. */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool Abandon(FName QuestId);

	/**
	 * Hands in a quest that is ready: checks CanTurnIn, takes consumed items, grants the rewards, marks it
	 * Completed (or starts its repeat cooldown) and unlocks the quests that depended on it. When refused because the
	 * inventory no longer completes it, the quest goes back to Active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool TurnIn(FName QuestId, FText& OutReason);

	/** Reports something that happened. Every active quest's counting objectives evaluate it. */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void ReportEvent(const FQuestEvent& Event);

	/** Re-reads every inventory objective of the active quests. Call it when items are lost, sold or used. */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void RefreshInventoryObjectives();

	/** Copies a quest's state into OutState. False if the quest is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool GetState(FName QuestId, FQuestRuntimeState& OutState) const;

	/** A quest's state, or null if it is unknown. Valid until the registry changes. */
	const FQuestRuntimeState* FindState(FName QuestId) const;

	/** Ids of every quest with the status, in registry order. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	TArray<FName> GetQuestsByStatus(EQuestStatus Status) const;

	/** The quest's definition, or null if it is unknown. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	UQuestDefinition* GetDefinition(FName QuestId) const;

	/** Every registered definition, in registry order. */
	const TArray<TObjectPtr<UQuestDefinition>>& GetAllDefinitions() const { return Definitions; }

	/** How many quests of the category are Active or ReadyToTurnIn. */
	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 CountActiveInCategory(FGameplayTag Category) const;

	// ---- Save readiness ----

	/** Every quest's state: the whole quest save. Store it with the game's own save system. */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	TArray<FQuestRuntimeState> GetAllStates() const;

	/**
	 * Replaces every state with a saved one. Generated bounties are removed first, then the saved ones are
	 * regenerated from their template and seed. Quests missing from the save start fresh; saved ids that no longer
	 * exist are skipped with a warning. Broadcasts OnQuestStatusChanged for every quest afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quest")
	void RestoreStates(const TArray<FQuestRuntimeState>& SavedStates);

	// ---- Events ----

	/** Fires when a quest's status changes. */
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestStatusChanged OnQuestStatusChanged;

	/** Fires when an objective's count changes. */
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestObjectiveProgress OnObjectiveProgress;

	/** Fires once per whole second of a timed quest's clock, and on accept. */
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestTimeChanged OnQuestTimeChanged;

	/** Fires when a quest leaves the registry. */
	UPROPERTY(BlueprintAssignable, Category = "Quest")
	FOnQuestUnregistered OnQuestUnregistered;

	// ---- FTickableGameObject: timers ----

	/** Counts down time limits and cooldowns by the world's delta time; not called while the game is paused. */
	virtual void Tick(float DeltaTime) override;

	/** Stat id for the tick. */
	virtual TStatId GetStatId() const override;

	/** Never for the class default object, otherwise conditional on IsTickable. */
	virtual ETickableTickType GetTickableTickType() const override;

	/** True once initialized while any quest has a running clock or cooldown. */
	virtual bool IsTickable() const override;

	/** The game instance's world, so the tick follows that world's pause and time dilation. */
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	/** One event waiting to be broadcast once the current call has finished changing state. */
	struct FPendingNotification
	{
		/** Which event. */
		enum class EKind : uint8 { Status, Progress, Time, Unregistered };

		/** Which event. */
		EKind Kind = EKind::Status;
		/** The quest. */
		FName QuestId;
		/** For Status: the new status. */
		EQuestStatus Status = EQuestStatus::Locked;
		/** For Progress: the objective. */
		int32 ObjectiveIndex = 0;
		/** For Progress: its count. */
		int32 Current = 0;
		/** For Progress: its required count. */
		int32 Required = 0;
		/** For Time: seconds left. */
		float Seconds = 0.f;
	};

	/** Registers every definition found in the settings' folders. */
	void LoadDefinitionsFromSettings();

	/** Index of the quest in Definitions and States, or INDEX_NONE. */
	int32 FindIndex(FName QuestId) const;

	/** True when every prerequisite quest has been completed at least once. Unknown ids count as missing. */
	bool ArePrerequisitesMet(const UQuestDefinition& Definition) const;

	/** A fresh state for the definition: Locked or Available, empty progress. */
	FQuestRuntimeState MakeInitialState(const UQuestDefinition& Definition) const;

	/** Sets a status and queues OnQuestStatusChanged if it changed. */
	void SetStatus(FQuestRuntimeState& State, EQuestStatus Status);

	/** Sets an objective's count (capped) and queues OnObjectiveProgress if it changed. */
	void SetProgress(const UQuestDefinition& Definition, FQuestRuntimeState& State, int32 ObjectiveIndex, int32 Value);

	/** True if the objective's count has reached its required count. */
	static bool IsObjectiveComplete(const UQuestDefinition& Definition, const FQuestRuntimeState& State, int32 ObjectiveIndex);

	/** True if the objective counts events now: always, or with bObjectivesInOrder only once every earlier one is complete. */
	static bool IsObjectiveCounting(const UQuestDefinition& Definition, const FQuestRuntimeState& State, int32 ObjectiveIndex);

	/** True if every objective is complete. */
	static bool AreObjectivesComplete(const UQuestDefinition& Definition, const FQuestRuntimeState& State);

	/** Re-reads the inventory objectives, in order; one not counting yet (bObjectivesInOrder) reads 0. */
	void RefreshInventoryProgress(const UQuestDefinition& Definition, FQuestRuntimeState& State);

	/** Read-only RefreshInventoryProgress: true if every objective would be complete after re-reading the inventory. */
	bool WouldObjectivesBeComplete(const UQuestDefinition& Definition, const FQuestRuntimeState& State) const;

	/** Moves an accepted quest between Active and ReadyToTurnIn. Returns true if it should now complete by itself. */
	bool UpdateReadiness(const UQuestDefinition& Definition, FQuestRuntimeState& State);

	/** Takes consumed items, grants rewards, sets Completed (or the cooldown) and unlocks dependants. Removes a generated quest. */
	bool CompleteQuest(FName QuestId, FText& OutReason);

	/** Time ran out: Failed, then the cooldown (repeatable) or back to Available. */
	void FailQuest(const UQuestDefinition& Definition, FQuestRuntimeState& State);

	/** Moves Locked quests whose prerequisites are now met to Available. */
	void UnlockQuests();

	/** Broadcasts every queued notification, in order. Re-entrant calls flush their own. */
	void FlushNotifications();

	/** The category's plural name for messages: the category limit's DisplayName, else the tag's last segment. */
	static FText GetCategoryText(const UQuestDefinition& Definition);

	/** Every quest definition, in registry order. Parallel to States. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UQuestDefinition>> Definitions;

	/** Every quest's state, parallel to Definitions. */
	TArray<FQuestRuntimeState> States;

	/** Events waiting for the current call to finish. */
	TArray<FPendingNotification> PendingNotifications;

	/** The game's inventory. */
	TWeakInterfacePtr<IQuestInventoryProvider> InventoryProvider;

	/** The game's reward granter. */
	TWeakInterfacePtr<IQuestRewardGranter> RewardGranter;

	/** The game's turn-in rule. */
	FQuestTurnInGate TurnInGate;

	/** True between Initialize and Deinitialize. */
	bool bInitialized = false;
};
