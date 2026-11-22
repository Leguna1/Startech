#include "UI/HUD/HudDraw.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Widget.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace HudDraw
{
	// Polyline points per 90 degrees of arc.
	constexpr int32 ArcPointsPerQuarter = 12;

	// Closer to the camera plane than this counts as behind it, where projection breaks down. In cm.
	constexpr double MinForwardDistance = 1.0;

	const FSlateBrush* GetWhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("GenericWhiteBox");
	}
}

void FHudDraw::Lines(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const TArray<FVector2f>& Points, const FLinearColor& Color, const float Thickness)
{
	if (Points.Num() < 2 || Color.A <= 0.f)
	{
		return;
	}
	FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true,
		Thickness);
}

void FHudDraw::Line(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const FVector2f& From,
	const FVector2f& To, const FLinearColor& Color, const float Thickness)
{
	Lines(Out, Layer, Geometry, { From, To }, Color, Thickness);
}

void FHudDraw::Box(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
	const FVector2f& Size, const FLinearColor& Color)
{
	BrushBox(Out, Layer, Geometry, Position, Size, *HudDraw::GetWhiteBrush(), Color);
}

void FHudDraw::BrushBox(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const FVector2f& Size, const FSlateBrush& Brush, const FLinearColor& Tint)
{
	if (Size.X <= 0.f || Size.Y <= 0.f || Tint.A <= 0.f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), &Brush,
		ESlateDrawEffect::None, Tint);
}

FVector2f FHudDraw::Measure(const FString& Text, const FSlateFontInfo& Font)
{
	if (Text.IsEmpty() || !FSlateApplication::IsInitialized())
	{
		return FVector2f::ZeroVector;
	}
	const TSharedRef<FSlateFontMeasure> Measurer = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FVector2f(Measurer->Measure(Text, Font));
}

FVector2f FHudDraw::Text(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const FString& Text,
	const FSlateFontInfo& Font, const FVector2f& Anchor, const FLinearColor& Color, const EHudTextAlign Align)
{
	const FVector2f Size = Measure(Text, Font);
	if (Size.IsZero() || Color.A <= 0.f)
	{
		return Size;
	}

	float X = Anchor.X;
	if (Align == EHudTextAlign::Centre)
	{
		X -= Size.X * 0.5f;
	}
	else if (Align == EHudTextAlign::Right)
	{
		X -= Size.X;
	}

	FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(X, Anchor.Y))),
		Text, Font, ESlateDrawEffect::None, Color);
	return Size;
}

FVector2f FHudDraw::TwoColourText(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FString& First, const FLinearColor& FirstColor, const FString& Second, const FLinearColor& SecondColor,
	const FSlateFontInfo& Font, const FVector2f& Anchor, const EHudTextAlign Align)
{
	// Measured as one string, so kerning across the join doesn't shift the second part.
	const FVector2f Whole = Measure(First + Second, Font);

	float X = Anchor.X;
	if (Align == EHudTextAlign::Centre)
	{
		X -= Whole.X * 0.5f;
	}
	else if (Align == EHudTextAlign::Right)
	{
		X -= Whole.X;
	}

	Text(Out, Layer, Geometry, First, Font, FVector2f(X, Anchor.Y), FirstColor, EHudTextAlign::Left);
	if (!Second.IsEmpty())
	{
		Text(Out, Layer, Geometry, Second, Font, FVector2f(X + Whole.X - Measure(Second, Font).X, Anchor.Y), SecondColor,
			EHudTextAlign::Left);
	}
	return Whole;
}

void FHudDraw::Arc(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
	const float Radius, const float StartDegrees, const float EndDegrees, const FLinearColor& Color, const float Thickness)
{
	const float Span = EndDegrees - StartDegrees;
	const int32 Steps = FMath::Max(2, FMath::CeilToInt(FMath::Abs(Span) / 90.f * HudDraw::ArcPointsPerQuarter));

	TArray<FVector2f> Points;
	Points.Reserve(Steps + 1);
	for (int32 Index = 0; Index <= Steps; ++Index)
	{
		const float Radians = FMath::DegreesToRadians(StartDegrees + Span * Index / Steps);
		Points.Add(Centre + FVector2f(FMath::Cos(Radians), FMath::Sin(Radians)) * Radius);
	}
	Lines(Out, Layer, Geometry, Points, Color, Thickness);
}

void FHudDraw::Ellipse(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
	const FVector2f& Radii, const FLinearColor& Color, const float Thickness)
{
	const int32 Steps = HudDraw::ArcPointsPerQuarter * 4;
	TArray<FVector2f> Points;
	Points.Reserve(Steps + 1);
	for (int32 Index = 0; Index <= Steps; ++Index)
	{
		const float Radians = UE_TWO_PI * Index / Steps;
		Points.Add(Centre + FVector2f(FMath::Cos(Radians) * Radii.X, FMath::Sin(Radians) * Radii.Y));
	}
	Lines(Out, Layer, Geometry, Points, Color, Thickness);
}

void FHudDraw::SegmentBar(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const FVector2f& Size, const int32 Segments, const float Gap,
	const TFunctionRef<FLinearColor(int32)> SegmentColor)
{
	if (Segments <= 0)
	{
		return;
	}

	const float Width = (Size.X - Gap * (Segments - 1)) / Segments;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		Box(Out, Layer, Geometry, Position + FVector2f((Width + Gap) * Index, 0.f), FVector2f(Width, Size.Y),
			SegmentColor(Index));
	}
}

void FHudDraw::Diamond(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
	const float HalfSize, const FLinearColor& Color, const float Thickness)
{
	const float R = HalfSize;
	Lines(Out, Layer, Geometry, {
		Centre + FVector2f(0.f, -R), Centre + FVector2f(R, 0.f), Centre + FVector2f(0.f, R),
		Centre + FVector2f(-R, 0.f), Centre + FVector2f(0.f, -R) }, Color, Thickness);
}

void FHudDraw::CornerBrackets(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Min, const FVector2f& Max, const float ArmLength, const FLinearColor& Color, const float Thickness)
{
	const float Arm = FMath::Min(ArmLength, FMath::Min(Max.X - Min.X, Max.Y - Min.Y) * 0.5f);
	CornerBrackets(Out, Layer, Geometry, Min, Max, FVector2f(Arm), Color, Thickness);
}

void FHudDraw::CornerBrackets(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Min, const FVector2f& Max, const FVector2f& ArmLength, const FLinearColor& Color,
	const float Thickness)
{
	const FVector2f Arm = FVector2f::Min(ArmLength, (Max - Min) * 0.5f);
	for (const bool bRight : { false, true })
	{
		for (const bool bBottom : { false, true })
		{
			const FVector2f Corner(bRight ? Max.X : Min.X, bBottom ? Max.Y : Min.Y);
			const float SignX = bRight ? -1.f : 1.f;
			const float SignY = bBottom ? -1.f : 1.f;
			Lines(Out, Layer, Geometry,
				{ Corner + FVector2f(SignX * Arm.X, 0.f), Corner, Corner + FVector2f(0.f, SignY * Arm.Y) }, Color, Thickness);
		}
	}
}

void FHudDraw::ChamferOutline(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const FVector2f& Size, const float Corner, const FLinearColor& Color, const float Thickness)
{
	const float C = FMath::Min(Corner, FMath::Min(Size.X, Size.Y) * 0.5f);
	const FVector2f P = Position;
	const FVector2f S = Size;
	Lines(Out, Layer, Geometry, {
		P,
		P + FVector2f(S.X - C, 0.f),
		P + FVector2f(S.X, C),
		P + S,
		P + FVector2f(C, S.Y),
		P + FVector2f(0.f, S.Y - C),
		P }, Color, Thickness);
}

bool FHudDraw::ProjectToWidget(const UWidget& Widget, const APlayerController& PC, const FGeometry& Geometry,
	const FVector& WorldLocation, FVector2f& OutPosition)
{
	FVector ViewLocation;
	FRotator ViewRotation;
	PC.GetPlayerViewPoint(ViewLocation, ViewRotation);
	if (FVector::DotProduct(WorldLocation - ViewLocation, ViewRotation.Vector()) <= HudDraw::MinForwardDistance)
	{
		return false;
	}

	FVector2D ViewportPixel;
	if (!PC.ProjectWorldLocationToScreen(WorldLocation, ViewportPixel, false))
	{
		return false;
	}

	// Viewport pixels to this widget's local space, which accounts for DPI scale and where the widget sits. Paint
	// geometry is relative to the window, so the window's position on the desktop comes off too: without it, everything
	// projected lands off screen in a game window that isn't at the desktop's corner (a maximized editor hid this).
	FVector2D Local;
	USlateBlueprintLibrary::ScreenToWidgetLocal(&Widget, Geometry, ViewportPixel, Local, true);
	OutPosition = FVector2f(Local);
	return true;
}

bool FHudDraw::IsInside(const FVector2f& Position, const FVector2f& Size)
{
	return Position.X >= 0.f && Position.Y >= 0.f && Position.X <= Size.X && Position.Y <= Size.Y;
}

float FHudDraw::Ease(const float Alpha)
{
	return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f));
}

FLinearColor FHudDraw::WithOpacity(const FLinearColor& Color, const float Opacity)
{
	FLinearColor Result = Color;
	Result.A *= Opacity;
	return Result;
}
