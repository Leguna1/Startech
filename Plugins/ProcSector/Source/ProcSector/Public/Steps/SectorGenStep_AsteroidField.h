#pragma once

#include "CoreMinimal.h"
#include "Core/SectorGenStep.h"
#include "SectorGenStep_AsteroidField.generated.h"

class AAsteroidField;

/**
 * Generation step that refills the asteroid field over several frames and
 * forwards "asteroid removed" events to the director so they are remembered.
 */
UCLASS()
class PROCSECTOR_API USectorGenStep_AsteroidField : public USectorGenStep
{
	GENERATED_BODY()

public:
	/** Returns "Asteroids". */
	virtual FName GetStepName() const override;

	/** Finds or spawns the field, subscribes to its removal event and starts a seeded fill sized by the sector's PlayAreaExtent. */
	virtual void Begin(const FSectorGenContext& Context) override;

	/** Adds asteroid batches until the field is full or the deadline passes. */
	virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override;

	/** Empties the field. */
	virtual void Clear(UWorld& World) override;

private:
	/** Receives the field's removal event and passes the id to the director for storage. */
	void HandleAsteroidRemoved(const int32 AsteroidId);

	/** Constant mixed into the sector seed for this step's random sequence. */
	static constexpr uint32 SeedSalt = 0xA57E401Du;

	/** The asteroid field this step drives. */
	TWeakObjectPtr<AAsteroidField> Field;

	/** Handle of the subscription to the field's removal event, kept so it is only bound once. */
	FDelegateHandle RemovedHandle;
};
