#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "QuestBridge.h"
#include "QuestTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "StartechQuestBridge.generated.h"

class AStartechEnemyShip;
class AStartechShipPawn;
class UAmmoData;
class UItemData;
class UQuestSubsystem;
class UTradeableData;

/** Log category for the quest bridge. */
STARTECH_API DECLARE_LOG_CATEGORY_EXTERN(LogStartechQuest, Log, All);

/**
 * Connects Startech to QuestFramework (Adapter). Implements the plugin's inventory provider and reward granter over
 * the player ship's cargo, ammo and credits, supplies the turn-in gate (docked only), and reports the game's quest
 * events to UQuestSubsystem:
 * - Quest.Event.TargetDestroyed: an enemy destroyed by the player ship (ReportEnemyDestroyed), subject = its EnemyTags.
 * - Quest.Event.ItemCollected: cargo or ammo gained (Amount = units gained), and cargo lost (sold, dropped, removed)
 *   with Amount = 0, so Collect objectives re-read the inventory. Ammo used by firing is not reported.
 * - Quest.Event.LocationReached: every ProcSector OnSectorReady, the first sector included; context = the sector id.
 * Events raised while the quest subsystem is calling into the bridge (granting rewards, taking turn-in items) are
 * held and reported on the next tick, so the plugin is never re-entered in the middle of a turn-in.
 * Also refreshes the bounty board each time the player docks.
 *
 * One per game world. It registers with the quest subsystem when the world begins play; the player ship binds
 * itself from its BeginPlay. The ship and the world both outlive a death (respawn moves and revives the same ship
 * and regenerates the same world), so neither registration has to be repeated after a respawn.
 */
UCLASS()
class STARTECH_API UStartechQuestBridge : public UWorldSubsystem, public IQuestInventoryProvider, public IQuestRewardGranter
{
	GENERATED_BODY()

public:
	/** The bridge of the ship's world, or null (for example in an editor world). */
	static UStartechQuestBridge* Get(const UObject* WorldContextObject);

	/** Registers the bridge and the turn-in gate with UQuestSubsystem and binds the sector director's OnSectorReady. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Clears the registrations, unbinds the director and the ship, and drops held events. */
	virtual void Deinitialize() override;

	/**
	 * Makes Ship the player ship quests read and reward: binds its cargo, ammo and docking events. Called by the ship
	 * from BeginPlay. A second ship replaces the first.
	 */
	void BindPlayerShip(AStartechShipPawn& Ship);

	/**
	 * While set, docking does not refresh the bounty board. The start flow sets it around placing a loaded game's
	 * ship at the station, so the saved offers survive; every other dock refreshes as usual.
	 */
	void SetDockRefreshSuppressed(bool bSuppressed) { bDockRefreshSuppressed = bSuppressed; }

	/**
	 * Called by every enemy at the moment it dies. Reports Quest.Event.TargetDestroyed only when Killer is the bound
	 * player ship (its shots and beams name it); any other or no killer is ignored.
	 */
	void ReportEnemyDestroyed(const AStartechEnemyShip& Enemy, const AActor* Killer);

	// ---- IQuestInventoryProvider ----

	/** Cargo held of the mineral with ItemTag, or whole rounds held of the ammo with ItemTag (exact tag). */
	virtual int32 GetItemCount(const FGameplayTag& ItemTag) const override;

	/** Takes Count of the mineral from the cargo, or Count rounds of the ammo. False if not enough is held. */
	virtual bool RemoveItems(const FGameplayTag& ItemTag, int32 Count) override;

	// ---- IQuestRewardGranter ----

	/**
	 * Reward.Credits adds credits. Any other tag grants the UStartechSettings::ItemCatalog entry with exactly that
	 * ItemTag: a mineral goes to the cargo up to its free space (the rest is logged and lost; the turn-in is not
	 * blocked), ammo to the weapon system, a weapon to the spare weapons.
	 */
	virtual void GrantReward(const FQuestReward& Reward) override;

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	/** OnSectorReady: reports Quest.Event.LocationReached with the sector id. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** The ship's OnDockedChanged: refreshes the bounty board on docking. */
	UFUNCTION()
	void HandleDockedChanged(bool bDocked, FText StationName);

	/** The cargo's OnItemCountChanged: reports Quest.Event.ItemCollected (Amount = gain, or 0 for a loss). */
	void HandleItemCountChanged(const UItemData* Item, int32 Delta);

	/** The weapon system's OnAmmoAdded: reports Quest.Event.ItemCollected with the whole rounds added. */
	void HandleAmmoAdded(const UAmmoData* Ammo, float Amount);

	/** The turn-in gate: only while the player ship is docked. */
	bool CanTurnIn(FName QuestId, FText& OutReason) const;

	/** Reports Quest.Event.ItemCollected for Good's ItemTag, warning once per asset that has no tag. */
	void ReportItemEvent(const UTradeableData& Good, int32 Amount);

	/** Reports Event now, or holds it for the next tick while the quest subsystem is calling into the bridge. */
	void SendEvent(const FQuestEvent& Event);

	/** Reports every held event, in order. */
	void FlushHeldEvents();

	/** Unbinds every event bound on the current ship. */
	void UnbindPlayerShip();

	/** The game instance's quest subsystem, or null. */
	UQuestSubsystem* GetQuests() const;

	/** The current ProcSector sector id, or None. */
	FName GetCurrentSectorId() const;

	/** The mineral in the ship's cargo whose ItemTag is exactly Tag, or null. */
	const UItemData* FindHeldItem(const FGameplayTag& Tag) const;

	/** The player ship quests read and reward. */
	TWeakObjectPtr<AStartechShipPawn> PlayerShip;

	/** The cargo binding on PlayerShip's inventory. */
	FDelegateHandle ItemCountHandle;

	/** The ammo binding on PlayerShip's weapon system. */
	FDelegateHandle AmmoAddedHandle;

	/** Events raised during a quest subsystem call, reported on the next tick. */
	TArray<FQuestEvent> HeldEvents;

	/** Above zero while the quest subsystem is inside GrantReward or RemoveItems. */
	int32 QuestCallDepth = 0;

	/** See SetDockRefreshSuppressed. */
	bool bDockRefreshSuppressed = false;

	/** Goods already warned about for having no ItemTag, so each is logged once. */
	TArray<TWeakObjectPtr<const UTradeableData>> WarnedUntagged;

	/** The pending next-tick flush of HeldEvents. */
	FTimerHandle FlushTimer;
};
