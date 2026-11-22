#pragma once

#include "CoreMinimal.h"
#include "DeathLootRecord.h"
#include "Subsystems/WorldSubsystem.h"
#include "DespawnSubsystem.generated.h"

class ALootPickup;
class USectorDirectorSubsystem;
struct FLootBundle;

/**
 * The single way enemies, stations and loot leave play, and the owner of loot dropped at runtime.
 * Sector-generated actors go back to ProcSector (which may remember the removal), dropped loot goes back to the pool,
 * and anything else (hand-placed actors, level spawners) is destroyed. Dropped loot is not part of any sector, so
 * every dropped pickup still in play is returned to the pool when a sector transition starts. Death cargo
 * (DropDeathLoot) is remembered first and put back when its sector is ready again, until it expires.
 */
UCLASS()
class STARTECH_API UDespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Removes a gameplay actor from play. Sector-spawned actors go back to the plugin (which may remember the removal),
	 * other pooled actors go back to the pool, and everything else is destroyed. While a sector is regenerating, the
	 * plugin cannot take actors back, so the call is retried next frame (the regeneration's teardown cancels the retry
	 * for actors it releases itself). Safe to call with null.
	 */
	static void Despawn(AActor* Actor);

	/**
	 * Takes a pickup of PickupClass from the pool at Location, fills it with Bundle and tracks it as dropped loot.
	 * Lifespan in seconds; 0 keeps it until collected or until the next sector transition.
	 * Returns null without acquiring anything if the bundle is empty, or if the pickup was collected at once.
	 */
	ALootPickup* DropLoot(TSubclassOf<ALootPickup> PickupClass, const FVector& Location, const FLootBundle& Bundle,
		float Lifespan);

	/**
	 * DropLoot for the player's death cargo. The pickup belongs to the current sector: warping away stores its
	 * contents and location, and arriving in that sector again puts it back. It expires Lifetime seconds of world
	 * time after this call, whichever sector the player is in; expired cargo is despawned or never restored.
	 * Lifetime 0 never expires.
	 */
	ALootPickup* DropDeathLoot(TSubclassOf<ALootPickup> PickupClass, const FVector& Location, const FLootBundle& Bundle,
		float Lifetime);

	/** Returns every dropped pickup still in play to the pool. Called when a sector transition starts. */
	void ReleaseAllDroppedLoot();

	/** Subscribes to the sector director's OnSectorTransitionStarted and OnSectorReady. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Unsubscribes from the sector director. */
	virtual void Deinitialize() override;

protected:
	/** Limits the subsystem to game and Play-In-Editor worlds, like the ProcSector subsystems it works with. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Runs the despawn checks in order: the sector, then dropped loot, then a retry while busy, then Destroy. */
	void DespawnNow(AActor& Actor);

	/** Removes Actor from the dropped-loot list. Returns true if it was in it. Also drops stale entries. */
	bool RemoveDroppedLoot(const AActor& Actor);

	/** Receives the director's OnSectorTransitionStarted: stores death cargo, then releases all dropped loot. */
	UFUNCTION()
	void HandleSectorTransitionStarted(FName FromSectorId, FName ToSectorId);

	/** Receives the director's OnSectorReady and restores the death cargo stored for that sector. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Copies the contents and location of every death pickup in play into its record and clears Pickup. */
	void StoreDeathLootInPlay();

	/** Acquires a pickup for every stored, unexpired record of SectorId, with the time it has left. */
	void RestoreDeathLoot(FName SectorId);

	/** Drops records that have expired, and records whose pickup left play (collected or timed out). */
	void PruneDeathLoot();

	/** World time used for death-cargo expiry. */
	double GetNow() const;

	/**
	 * Puts Pickup on the HUD as death cargo: its map marker (added at runtime if the Blueprint has none) is set to
	 * DroppedCargo with a screen marker, given ExpiryTime (negative for none) and registered.
	 */
	static void ShowDeathCargoMarker(ALootPickup& Pickup, double ExpiryTime);

	/** Takes Pickup's death-cargo marker off the HUD: unregisters it and clears its expiry. Safe without one. */
	static void HideDeathCargoMarker(ALootPickup& Pickup);

	/** Live pickups this subsystem took from the pool. Exactly the actors that are pooled but not owned by a sector. */
	TArray<TWeakObjectPtr<ALootPickup>> DroppedLoot;

	/** Death cargo, in play or stored for another sector. Its pickups in play are also in DroppedLoot. */
	UPROPERTY()
	TArray<FDeathLootRecord> DeathLoot;

	/** Director this subsystem is subscribed to, kept so Deinitialize can unsubscribe. */
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;
};
