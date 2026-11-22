#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootBundle.h"
#include "Population/Poolable.h"
#include "LootPickup.generated.h"

class ALootPickup;
class ULootTable;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLootPickupCollected, ALootPickup*, Pickup, AActor*, Collector);

// Loot floating in space. Any overlapping actor that implements ILootCollector picks it up.
// Whatever the collector can't take stays here until it has room.
// Pooled: sector loot and dropped loot are reused, so per-life state is reset in ResetForNewLife.
UCLASS()
class STARTECH_API ALootPickup : public AActor, public IPoolable
{
	GENERATED_BODY()

public:
	ALootPickup();

	const FLootBundle& GetContents() const { return Contents; }
	void SetContents(const FLootBundle& NewContents);

	/** Despawns the pickup after Seconds unless it is collected first. The pool clears the timer on release. */
	void StartDespawnTimer(float Seconds);

	/** Pool hook: resets the pickup for a new life (ResetForNewLife). */
	virtual void OnAcquiredFromPool_Implementation() override;

	/** Pool hook: lets go of waiting collectors and empties the contents, so a reused pickup starts fresh. */
	virtual void OnReleasedToPool_Implementation() override;

	// Everything was taken. The pickup is despawned right after.
	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FOnLootPickupCollected OnCollected;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	// Overlaps pawns only, so it never blocks ships or weapon traces.
	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<USphereComponent> PickupSphere;

	// Visual only, no collision. Assign the mesh in the Blueprint.
	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Loot")
	FLootBundle Contents;

	// Rolled with the spawn sector's tier (UStartechTierSubsystem) at the start of each life (every spawn and respawn)
	// if Contents is still empty, up to
	// MaxRollAttempts times until something comes out. Zone loot pickups need one; drop pickups are filled by the dropper.
	UPROPERTY(EditAnywhere, Category = "Loot")
	TObjectPtr<ULootTable> LootTable;

	// Rolls of LootTable per life before an empty pickup gives up and despawns.
	static constexpr int32 MaxRollAttempts = 20;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// A waiting collector's cargo changed, so it may have room now.
	UFUNCTION()
	void HandleCollectorItemsChanged();

	void TryCollect(AActor* Collector);
	void TryCollectFromOverlapping();

	// Collectors still inside that couldn't take everything. Retried only when their inventory changes.
	void StartWaiting(AActor* Collector);
	void StopWaiting(AActor* Collector);
	void StopWaitingForAll();

	/**
	 * Per-life setup, run from BeginPlay (hand-placed and first spawn) and OnAcquiredFromPool. Idempotent.
	 * Clears the collected flags, rolls LootTable (up to MaxRollAttempts times) if Contents is empty, and schedules
	 * Settle for next frame.
	 */
	void ResetForNewLife();

	/**
	 * Runs one frame after a new life starts, once whoever spawned the pickup has filled it and its overlaps are
	 * current: despawns it if it is still empty, otherwise collects for anything already inside.
	 */
	void Settle();

	/** Fired by StartDespawnTimer. */
	void HandleDespawnTimerElapsed();

	TArray<TWeakObjectPtr<AActor>> WaitingCollectors;

	/** Pending Settle call, so a life that starts twice in one frame (first pooled spawn) settles once. */
	FTimerHandle SettleTimer;

	/** Despawn timeout started by StartDespawnTimer. */
	FTimerHandle DespawnTimer;

	// Set while handing loot over, so inventory events raised by that hand-over don't re-enter.
	bool bCollecting = false;
	bool bCollected = false;
};
