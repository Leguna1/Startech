#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"
#include "Templates/Function.h"

class APlayerController;
class UWidget;
class FSlateWindowElementList;
struct FSlateBrush;

/** Horizontal placement of a line of HUD text relative to its anchor point. */
enum class EHudTextAlign : uint8
{
	/** The anchor is the text's left edge. */
	Left,
	/** The anchor is the text's centre. */
	Centre,
	/** The anchor is the text's right edge. */
	Right
};

/**
 * The drawing primitives every HUD element paints with: lines, solid boxes, text, arcs, segmented bars, diamonds,
 * brackets and cut-corner outlines, all as FSlateDrawElement calls in widget-local space. No textures needed.
 */
class STARTECH_API FHudDraw
{
public:
	/** A polyline through Points. */
	static void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points,
		const FLinearColor& Color, float Thickness);

	/** One straight line. */
	static void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& From,
		const FVector2f& To, const FLinearColor& Color, float Thickness);

	/** A solid box with its top-left corner at Position. */
	static void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, const FLinearColor& Color);

	/** A box filled with Brush (a texture or a material), tinted by Tint. */
	static void BrushBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, const FSlateBrush& Brush, const FLinearColor& Tint);

	/** Size of Text in Font, in widget units. Zero when Slate isn't running. */
	static FVector2f Measure(const FString& Text, const FSlateFontInfo& Font);

	/** One line of text whose top edge is at Anchor.Y, placed horizontally by Align. Returns its size. */
	static FVector2f Text(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FString& Text,
		const FSlateFontInfo& Font, const FVector2f& Anchor, const FLinearColor& Color, EHudTextAlign Align);

	/**
	 * One line in two colours, First then Second, placed as a whole by Align with its top edge at Anchor.Y. Returns
	 * the size of the whole line. An empty Second draws First alone.
	 */
	static FVector2f TwoColourText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
		const FString& First, const FLinearColor& FirstColor, const FString& Second, const FLinearColor& SecondColor,
		const FSlateFontInfo& Font, const FVector2f& Anchor, EHudTextAlign Align);

	/** An arc from StartDegrees to EndDegrees (screen angles: 0 is right, 90 is down), as a polyline. */
	static void Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
		float Radius, float StartDegrees, float EndDegrees, const FLinearColor& Color, float Thickness);

	/** A closed ellipse with radii Radii. */
	static void Ellipse(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
		const FVector2f& Radii, const FLinearColor& Color, float Thickness);

	/**
	 * Segments boxes side by side across Size, separated by Gap, coloured by SegmentColor(Index) from left to right.
	 */
	static void SegmentBar(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, int32 Segments, float Gap, TFunctionRef<FLinearColor(int32)> SegmentColor);

	/** A hollow diamond with its points HalfSize from Centre. */
	static void Diamond(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
		float HalfSize, const FLinearColor& Color, float Thickness);

	/** Four open corner brackets around the rectangle Min..Max, each arm ArmLength long. */
	static void CornerBrackets(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min,
		const FVector2f& Max, float ArmLength, const FLinearColor& Color, float Thickness);

	/**
	 * Four open corner brackets around the rectangle Min..Max: horizontal arms ArmLength.X long, vertical arms
	 * ArmLength.Y long, each at most half the rectangle's side.
	 */
	static void CornerBrackets(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min,
		const FVector2f& Max, const FVector2f& ArmLength, const FLinearColor& Color, float Thickness);

	/** The outline of a rectangle with its top-right and bottom-left corners cut at 45 degrees by Corner. */
	static void ChamferOutline(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, float Corner, const FLinearColor& Color, float Thickness);

	/**
	 * Where WorldLocation falls in Widget's local space, using the player's view. False if it is behind the camera or
	 * can't be projected; true does not mean it is inside the widget.
	 */
	static bool ProjectToWidget(const UWidget& Widget, const APlayerController& PC, const FGeometry& Geometry,
		const FVector& WorldLocation, FVector2f& OutPosition);

	/** True when Position lies inside a widget of local size Size. */
	static bool IsInside(const FVector2f& Position, const FVector2f& Size);

	/** Smoothstep ease of Alpha, clamped to 0-1. */
	static float Ease(float Alpha);

	/** Color with its alpha multiplied by Opacity. */
	static FLinearColor WithOpacity(const FLinearColor& Color, float Opacity);
};
