#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubSliderRowWidget.generated.h"

class USizeBox;

/** Broadcast with the slider's new value, 0-1. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHubSliderValue, float);

/**
 * One labelled slider of the hub's style: the label, a track with its filled part and a thumb, and the value as a
 * percentage ("70%"). Painted in C++ from DA_HudStyle (Player colour fill, the empty-segment colour for the rest).
 * Drag or click on the track; the Left and Right arrow keys step 5% while it has focus. Values snap to 1%.
 * OnValueChanged fires while the value moves; OnValueCommitted when a drag ends or a key step is made.
 */
UCLASS()
class STARTECH_API UHubSliderRowWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse and keyboard focus. */
	UHubSliderRowWidget(const FObjectInitializer& ObjectInitializer);

	void SetLabel(const FText& InLabel);

	FText GetLabel() const { return Label; }

	/** Sets the value (clamped to 0-1, snapped to 1%) without firing the delegates. */
	void SetValue(float InValue);

	float GetValue() const { return Value; }

	/** The value as drawn ("70%"). */
	FString GetValueText() const;

	/** Where Fraction (0-1) of the track is, in absolute (desktop) coordinates, for tests that drag it for real. */
	FVector2D GetAbsoluteTrackPoint(float Fraction) const;

	/** While the value moves (a drag, a click, a key step). */
	FOnHubSliderValue OnValueChanged;

	/** When the player lets go: a drag ends, or a key step. */
	FOnHubSliderValue OnValueCommitted;

protected:
	/** A size box giving the row its height. */
	virtual void BuildContent() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** The track's left edge and width in local units, from a size. */
	static void GetTrack(const FVector2f& Size, float& OutLeft, float& OutWidth);

	/** Sets the value from a local X and fires OnValueChanged if it changed. */
	void SetFromLocalX(const FGeometry& Geometry, float LocalX);

	/** Ends a drag: releases and commits. */
	void EndDrag();

	FText Label;
	float Value = 0.f;
	bool bHovered = false;
	bool bDragging = false;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SizeBox;
};
