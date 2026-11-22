#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HudElementWidget.h"
#include "RadarWidget.generated.h"

class UMapMarkerComponent;

// A 3D radar centred on the player, as if seen from above at an angle: an ellipse (2.2 : 1) with an inner ring at half
// range and two cross lines. Each contact sits at its position relative to the player, rotated by the player's yaw, on
// the ellipse plane, with a vertical stalk up or down to its height and a dot coloured by kind (hostile, loot, neutral).
// Contacts beyond range aren't drawn. Paints itself every frame; give it a size in the layout.
// Contacts are gathered from UMapMarkerSubsystem exactly as before the HUD rework.
UCLASS()
class STARTECH_API URadarWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	URadarWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	// The distance at the ellipse's rim, in cm, measured horizontally. Shown under the radar (150 000 = "1.5 km").
	UPROPERTY(EditDefaultsOnly, Category = "Radar", meta = (ClampMin = "1"))
	float Range = 150000.f;

private:
	/** Markers to show: enabled, on the radar, not the player, and with an owner in play (not hidden). */
	void GatherContacts(const APawn& Pawn, TArray<const UMapMarkerComponent*>& OutContacts) const;
};
