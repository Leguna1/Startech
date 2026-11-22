#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SectorTypes.h"
#include "JumpGate.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A trigger that starts a warp to another sector when a pawn carrying a
 * UWarpTransitionComponent flies into it. Gates are pooled and reconfigured for each sector;
 * the gate itself knows nothing about generation, it only asks the pawn's component to warp.
 */
UCLASS()
class PROCSECTOR_API AJumpGate : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the trigger, placeholder ring mesh and destination label. */
	AJumpGate();

	/**
	 * Moves the gate to WorldLocation, turns it toward the level centre and sets its destination and label from the link.
	 * WorldLocation is the link's NormalizedLocation resolved against the sector's play area (FPlayAreaMath::ToWorld).
	 */
	void Configure(const FGateLink& Link, const FVector& WorldLocation, const FText& DestinationLabel);

	/** Sector this gate currently leads to. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Gate")
	FName GetDestinationSectorId() const;

protected:
	/** Binds the trigger's overlap event. */
	virtual void BeginPlay() override;

	/**
	 * Starts a warp on the overlapping pawn's UWarpTransitionComponent, if it has one, through BeginWarpFromTrigger:
	 * a warp the game refuses is retried while the pawn stays inside the trigger.
	 */
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Overlap volume the ship must enter. Root component. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Trigger;

	/** Placeholder visual (a flattened cylinder). Replace the mesh in a Blueprint subclass. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	/** Floating text showing the destination's display name. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UTextRenderComponent> Label;

	/** Sector this gate leads to. Set by Configure; editable for gates placed by hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FName DestinationSectorId;
};
