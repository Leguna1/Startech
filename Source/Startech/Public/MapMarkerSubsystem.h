#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MapMarkerSubsystem.generated.h"

class UMapMarkerComponent;

/** Broadcast when the marker list or a marker's enabled flag changes. Marker is the one that changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarkersChanged, UMapMarkerComponent*, Marker);

// Every map marker in the world, so the radar and screen markers never have to search actors.
UCLASS()
class STARTECH_API UMapMarkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Adds Marker. Idempotent: registering a marker already in the list changes nothing and fires nothing. */
	void Register(UMapMarkerComponent* Marker);

	/** Removes Marker if it is in the list, and drops stale entries. Fires OnMarkersChanged only if it was there. */
	void Unregister(UMapMarkerComponent* Marker);

	/** Called by a registered marker whose enabled flag changed; fires OnMarkersChanged. */
	void NotifyMarkerChanged(UMapMarkerComponent* Marker);

	// May contain stale entries; check each one before use.
	const TArray<TWeakObjectPtr<UMapMarkerComponent>>& GetMarkers() const { return Markers; }

	/**
	 * Fires when a marker is added, removed, enabled or disabled. Markers also go out of play by their owner being
	 * hidden (dead or pooled), which is not announced here; check the owner's visibility when drawing.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Markers")
	FOnMarkersChanged OnMarkersChanged;

private:
	/** True when Marker is in the list. */
	bool Contains(const UMapMarkerComponent* Marker) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UMapMarkerComponent>> Markers;
};
