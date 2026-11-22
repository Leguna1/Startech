#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MapMarkerType.h"
#include "RadarContactKind.h"
#include "Tiers/SectorTierTypes.h"
#include "MapMarkerComponent.generated.h"

// Shows its owner on the radar and, optionally, as an on-screen navigation marker. Its location is the marker's.
// Registers itself with UMapMarkerSubsystem from BeginPlay to EndPlay. Pooled owners can also register and unregister
// it explicitly with RegisterMarker and UnregisterMarker; both are idempotent.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UMapMarkerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// A disabled marker is drawn nowhere. A real change is announced through UMapMarkerSubsystem::OnMarkersChanged.
	void SetMarkerEnabled(bool bNewEnabled);
	bool IsMarkerEnabled() const { return bMarkerEnabled; }

	/** Adds this marker to the world's marker list. Safe to call again while already registered. */
	void RegisterMarker();

	/** Removes this marker from the world's marker list. Safe to call when not registered. */
	void UnregisterMarker();

	/** Gives the marker an expiry at WorldTime (UWorld::GetTimeSeconds); negative removes it. */
	void SetExpiryTime(double WorldTime);

	/** True while the marker has an expiry time. */
	UFUNCTION(BlueprintPure, Category = "Marker")
	bool HasExpiry() const { return ExpiryTime >= 0.0; }

	/** Seconds until the expiry time, never below 0; 0 when there is no expiry (check HasExpiry). */
	UFUNCTION(BlueprintPure, Category = "Marker")
	float GetSecondsRemaining() const;

	/** Hostile for enemies, Loot for loot and dropped cargo, Neutral for everything else. */
	UFUNCTION(BlueprintPure, Category = "Marker")
	ERadarContactKind GetContactKind() const;

	/** True when the owner has a UTargetableComponent whose targetable flag is set. */
	UFUNCTION(BlueprintPure, Category = "Marker")
	bool IsOwnerTargetable() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	EMapMarkerType Type = EMapMarkerType::Objective;

	// Shown beside the screen marker.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	FText Label;

	/** Drawn after Label on the screen marker, separated by " · ", in LabelSuffixColor ("Tier III" on gates). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	FText LabelSuffix;

	/** The style colour of LabelSuffix. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker")
	ETierColorRole LabelSuffixColor = ETierColorRole::Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|Radar")
	bool bShowOnRadar = true;

	// In cm, measured horizontally. 0 uses the radar's own range; otherwise the marker only shows within this distance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|Radar", meta = (ClampMin = "0"))
	float RadarRange = 0.f;

	// Beyond its range, drawn on the radar's rim in its direction instead of being hidden.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|Radar")
	bool bPinToRadarRim = false;

	// Drawn on screen with its label and distance, or as an edge arrow while off screen.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|Screen")
	bool bShowScreenMarker = false;

	// Among screen markers of the same type with this set, only the one nearest the player is shown.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker|Screen")
	bool bNearestOnly = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	// Changed at runtime through SetMarkerEnabled, e.g. when the owner dies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker", meta = (AllowPrivateAccess = "true"))
	bool bMarkerEnabled = true;

	/** World time the marker expires at, for a countdown (death cargo). Negative for none. Set at runtime only. */
	double ExpiryTime = -1.0;
};
