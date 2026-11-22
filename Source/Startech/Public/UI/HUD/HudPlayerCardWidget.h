#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/HUD/HudElementWidget.h"
#include "UI/HUD/HudPanelBrush.h"
#include "HudPlayerCardWidget.generated.h"

class UHealthComponent;
class UTexture2D;

/**
 * Top left: the player's portrait, callsign and ship id, and segmented shield and hull bars. A lost segment flickers
 * for the style's flicker time before staying empty; restored segments fill at once. The hull bar pulses while hull
 * is low. Event-driven: wrap it in an Invalidation Box.
 */
UCLASS()
class STARTECH_API UHudPlayerCardWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots first after a warp. */
	UHudPlayerCardWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the new pawn's health component. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads the callsign, ship id, shield, hull and low-hull state; fills the bars without flicker. */
	virtual void RefreshFromPawn() override;

	/** Binds to the player profile for the callsign. */
	virtual void NativeConstruct() override;

	/** Unbinds from the profile. */
	virtual void NativeDestruct() override;

	/** Repaints while a segment flickers or the hull pulses. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Rebuilds the panel material instance. */
	virtual void HandleStyleChanged() override;

	/** The player ship's portrait, made by Tools/HUD/capture_ship_portrait.py. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UTexture2D> Portrait;

private:
	/** One segmented gauge: its value and, per segment, when its loss flicker ends. */
	struct FGauge
	{
		/** Segments currently filled. */
		int32 Filled = 0;
		/** Per segment: HUD time its flicker ends; 0 when not flickering. */
		TArray<double> FlickerUntil;
	};

	UFUNCTION()
	void HandleCallsignChanged(FText NewCallsign);

	UFUNCTION()
	void HandleShieldChanged(float Current, float Max);

	UFUNCTION()
	void HandleHullChanged(float Current, float Max);

	UFUNCTION()
	void HandleLowHullChanged(bool bIsLow);

	/** Applies a new value to Gauge, starting a flicker on each lost segment. */
	void UpdateGauge(FGauge& Gauge, float Current, float Max) const;

	/** Sets Gauge to a value at once: the style's segment count, no flicker. */
	void SetGaugeImmediately(FGauge& Gauge, float Current, float Max) const;

	/** Segments of Segments lit for Current of Max; any remaining sliver keeps its segment lit. */
	static int32 GetFilledSegments(float Current, float Max, int32 Segments);

	/** Reads the callsign and ship id from the profile. */
	void RefreshIdentity();

	/** True while any segment of Gauge flickers. */
	static bool IsFlickering(const FGauge& Gauge, double Now);

	/** Draws Gauge as a segment bar coloured FillColor, pulsing it when bPulse. */
	void DrawGauge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, const FGauge& Gauge, const FLinearColor& FillColor, bool bPulse) const;

	/** Unbinds from the bound health component. */
	void UnbindHealth();

	/** The health component the bars follow. */
	TWeakObjectPtr<UHealthComponent> BoundHealth;

	/** Panel background. */
	UPROPERTY(Transient)
	FHudPanelBrush Panel;

	/** Brush for the portrait texture. */
	FSlateBrush PortraitBrush;

	FGauge ShieldGauge;
	FGauge HullGauge;

	/** Cached callsign. */
	FText Callsign;

	/** Cached ship id. */
	FText ShipId;

	/** Cached low-hull state. */
	bool bLowHull = false;
};
