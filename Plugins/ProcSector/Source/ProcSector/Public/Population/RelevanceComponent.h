#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Population/Poolable.h"
#include "RelevanceComponent.generated.h"

/** Broadcast when the owner wakes up (true) or goes to sleep (false). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelevanceChanged, bool, bIsAwake);

/**
 * Add to any actor that should stop costing CPU while the player is far away.
 * URelevanceSubsystem decides when to check; this component decides, from its own
 * distances, whether its owner sleeps, and applies the result to the owner.
 * Bind OnRelevanceChanged to pause anything else, such as AI or looping effects.
 */
UCLASS(ClassGroup = (ProcSector), meta = (BlueprintSpawnableComponent))
class PROCSECTOR_API URelevanceComponent : public UActorComponent, public IPoolable
{
	GENERATED_BODY()

public:
	/** Disables this component's own tick; the subsystem drives it instead. */
	URelevanceComponent();

	/** Compares the owner's distance to the viewer against the thresholds and sleeps or wakes the owner. */
	void Evaluate(const FVector& ViewerLocation);

	/**
	 * Forces the awake state and applies it to the owner. Broadcasts OnRelevanceChanged when it changes.
	 * Asleep, the owner's tick and every component's tick are off; awake, each is restored to its bStartWithTickEnabled.
	 */
	void SetAwake(const bool bNewAwake);

	/** True while the owner is awake. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Relevance")
	bool IsAwake() const;

	/** False while the owner is stored in the pool, during which the subsystem skips it. */
	bool IsEvaluationEnabled() const;

	/** Pool hook: resumes evaluation and treats the freshly activated owner as awake. */
	virtual void OnAcquiredFromPool_Implementation() override;

	/** Pool hook: suspends evaluation so a stored actor is never woken. */
	virtual void OnReleasedToPool_Implementation() override;

	/** Fires when the owner wakes or sleeps. */
	UPROPERTY(BlueprintAssignable, Category = "ProcSector|Relevance")
	FOnRelevanceChanged OnRelevanceChanged;

protected:
	/** Registers with the relevance subsystem and remembers whether the owner ticks. */
	virtual void BeginPlay() override;

	/** Unregisters from the relevance subsystem. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Distance beyond which the owner goes to sleep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relevance", meta = (ClampMin = "0.0"))
	float SleepDistance = 45000.0f;

	/** Distance within which the owner wakes. Keep it below SleepDistance so the state does not flicker at the edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relevance", meta = (ClampMin = "0.0"))
	float WakeDistance = 40000.0f;

	/** True turns the owner's collision off while asleep. Leave false for anything that must stay targetable from afar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relevance")
	bool bDisableCollisionWhenAsleep = false;

	/** True hides the owner while asleep. Leave false when it should stay visible in the distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relevance")
	bool bHideWhenAsleep = false;

private:
	/** Current state. Owners start awake. */
	bool bAwake = true;

	/** False while the owner is stored in the pool. */
	bool bEvaluationEnabled = true;

	/** Whether the owner's tick was enabled at BeginPlay, so waking only re-enables ticks that existed. */
	bool bOwnerTicksWhenAwake = false;
};
