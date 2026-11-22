#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayAreaSyncComponent.generated.h"

class UBoxComponent;
class USectorDirectorSubsystem;

/**
 * Keeps a bounds box the size of the current sector's play area. Add it to any actor that owns a UBoxComponent
 * (for example the host game's sector-bounds actor): whenever a sector becomes ready, the box's unscaled extent is
 * set to USectorDirectorSubsystem::GetPlayAreaExtent plus ExtentPadding. The actor should sit at the level origin
 * with scale 1, because the play area is centred on the origin. It only applies the size; what the box does with
 * the player is left to its owner.
 */
UCLASS(ClassGroup = (ProcSector), meta = (BlueprintSpawnableComponent))
class PROCSECTOR_API UPlayAreaSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Disables ticking; the component only reacts to sector changes. */
	UPlayAreaSyncComponent();

	/** Sets the box's unscaled extent to the director's play-area extent plus ExtentPadding. Logs a warning once if no box is found. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector")
	void ApplyPlayArea();

protected:
	/** Subscribes to the director's OnSectorReady and applies the play area at once if a sector is already current. */
	virtual void BeginPlay() override;

	/** Unsubscribes from the director. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Name of the UBoxComponent on the owner to resize. None uses the owner's first UBoxComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Area")
	FName BoxComponentName;

	/** Added to the play-area extent, so the bounds warning starts just outside the generated content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Play Area", meta = (ClampMin = "0.0"))
	FVector ExtentPadding = FVector(10000.0, 10000.0, 5000.0);

private:
	/** Receives the director's OnSectorReady and applies the new sector's play area. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Returns the box to resize: the one named BoxComponentName, or the owner's first UBoxComponent when the name is None. */
	UBoxComponent* FindBox() const;

	/** Director this component is subscribed to, kept so EndPlay can unsubscribe. */
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;

	/** True once the missing-box warning has been logged, so it is not repeated on every sector change. */
	bool bWarnedMissingBox = false;
};
