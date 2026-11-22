#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GateMarkerSubsystem.generated.h"

class AJumpGate;
class UMapMarkerComponent;
class USectorDirectorSubsystem;

/**
 * Puts ProcSector's jump gates on the radar and screen markers without changing the plugin. On every OnSectorReady it
 * gives each gate in play a UMapMarkerComponent (added at runtime the first time, reused after), labelled
 * "Gate · <destination>" with the destination's tier as the label suffix ("Tier III", in the tier's colour), and
 * unregisters the markers of gates the pool has stored away.
 */
UCLASS()
class STARTECH_API UGateMarkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Subscribes to the sector director's OnSectorReady. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Unsubscribes from the sector director. */
	virtual void Deinitialize() override;

protected:
	/** Limits the subsystem to game and Play-In-Editor worlds, like the ProcSector subsystems it works with. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Receives the director's OnSectorReady and refreshes every gate's marker. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** The marker this subsystem added to Gate, creating it if it doesn't exist yet. */
	static UMapMarkerComponent* FindOrAddMarker(AJumpGate& Gate);

	/** The marker this subsystem added to Gate, or null. */
	static UMapMarkerComponent* FindMarker(const AJumpGate& Gate);

	/** Sets Marker's label suffix to the tier of Gate's destination ("Tier III") and its colour. */
	void ApplyTier(const AJumpGate& Gate, UMapMarkerComponent& Marker) const;

	/** "Gate · " plus the destination's display name from the catalog (its id if the catalog doesn't know it). */
	FText MakeLabel(const AJumpGate& Gate) const;

	/** Director this subsystem is subscribed to, kept so Deinitialize can unsubscribe. */
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;
};
