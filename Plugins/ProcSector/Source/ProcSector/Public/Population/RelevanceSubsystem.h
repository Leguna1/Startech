#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RelevanceSubsystem.generated.h"

class URelevanceComponent;

/**
 * Checks a fixed number of relevance components per frame, in rotation, against the player's position.
 * The per-frame cost is constant regardless of actor count; more actors only means each one is checked less often.
 */
UCLASS()
class PROCSECTOR_API URelevanceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Adds a component to the rotation. Called by URelevanceComponent::BeginPlay. */
	void Register(URelevanceComponent& Component);

	/** Removes a component from the rotation. Called by URelevanceComponent::EndPlay. */
	void Unregister(URelevanceComponent& Component);

	/** Evaluates the next batch of components against the player pawn's location. */
	virtual void Tick(float DeltaTime) override;

	/** Stat id required by the tickable interface. */
	virtual TStatId GetStatId() const override;

	/** Number of registered components that are valid and not stored in the pool. */
	int32 GetTrackedCount() const;

	/** Number of tracked components whose owner is currently awake. */
	int32 GetAwakeCount() const;

protected:
	/** Limits the subsystem to game and Play-In-Editor worlds. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Finds the location relevance is measured from: the first player's pawn, or its camera when there is no pawn. */
	bool GetViewerLocation(FVector& OutLocation) const;

	/** Every registered component. Weak so destroyed owners drop out on their own. */
	TArray<TWeakObjectPtr<URelevanceComponent>> Components;

	/** Index of the next component to evaluate. */
	int32 Cursor = 0;
};
