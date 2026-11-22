#pragma once

#include "CoreMinimal.h"
#include "MapMarkerType.h"
#include "UI/HUD/HudElementWidget.h"
#include "NavMarkerLayerWidget.generated.h"

class APlayerController;
class UMapMarkerComponent;

// A full-screen, click-through layer that marks map markers with bShowScreenMarker while they are on screen: a small
// hollow diamond, the label below it (gates and death cargo in the Credits colour, stations in the Player colour), and
// the distance below that in secondary text, with the time left for markers that expire ("860 m · 1:42").
// Nothing is drawn for markers off screen, behind the camera, or closer than the style's MarkerHideDistance.
// Paints itself every frame; stretch it over the whole screen.
UCLASS()
class STARTECH_API UNavMarkerLayerWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	UNavMarkerLayerWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	// Enabled screen markers, minus those beaten by a nearer marker of the same type when bNearestOnly.
	void GatherMarkers(const FVector& PlayerLocation, TArray<const UMapMarkerComponent*>& OutMarkers) const;

	/** The label colour for a marker type: Credits for gates and death cargo, Player for stations, else Text. */
	FLinearColor GetLabelColor(EMapMarkerType Type) const;

	/** The distance line: the distance, plus " · m:ss" for a marker with an expiry. */
	static FText MakeDistanceLine(const UMapMarkerComponent& Marker, float Distance);
};
