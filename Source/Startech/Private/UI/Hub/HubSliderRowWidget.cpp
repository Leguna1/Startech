#include "UI/Hub/HubSliderRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace HubSlider
{
	constexpr float Height = 34.f;
	constexpr float MinWidth = 460.f;
	constexpr float LabelWidth = 140.f;
	constexpr float ValueWidth = 56.f;
	constexpr float Gap = 14.f;
	constexpr float TrackHeight = 6.f;
	constexpr float ThumbWidth = 10.f;
	constexpr float ThumbHeight = 20.f;
	constexpr float KeyStep = 0.05f;
	constexpr float Snap = 0.01f;
}

UHubSliderRowWidget::UHubSliderRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UHubSliderRowWidget::BuildContent()
{
	SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetHeightOverride(HubSlider::Height);
	SizeBox->SetMinDesiredWidth(HubSlider::MinWidth);
	WidgetTree->RootWidget = SizeBox;
}

void UHubSliderRowWidget::SetLabel(const FText& InLabel)
{
	Label = InLabel;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHubSliderRowWidget::SetValue(const float InValue)
{
	Value = FMath::GridSnap(FMath::Clamp(InValue, 0.f, 1.f), HubSlider::Snap);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString UHubSliderRowWidget::GetValueText() const
{
	return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f));
}

void UHubSliderRowWidget::GetTrack(const FVector2f& Size, float& OutLeft, float& OutWidth)
{
	OutLeft = HubSlider::LabelWidth + HubSlider::Gap;
	OutWidth = FMath::Max(Size.X - OutLeft - HubSlider::Gap - HubSlider::ValueWidth, 1.f);
}

FVector2D UHubSliderRowWidget::GetAbsoluteTrackPoint(const float Fraction) const
{
	const FGeometry& Geometry = GetCachedGeometry();
	float Left = 0.f;
	float Width = 0.f;
	GetTrack(FVector2f(Geometry.GetLocalSize()), Left, Width);
	const FVector2f Local(Left + Width * FMath::Clamp(Fraction, 0.f, 1.f), Geometry.GetLocalSize().Y * 0.5f);
	return FVector2D(Geometry.LocalToAbsolute(FVector2D(Local)));
}

int32 UHubSliderRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled) + 1;
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, true);
	const float TextTop = (Size.Y - FHudDraw::Measure(TEXT("0%"), Font).Y) * 0.5f;
	const bool bActive = bHovered || bDragging || HasKeyboardFocus();

	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Label.ToString(), Font, FVector2f(0.f, TextTop),
		bActive ? HudStyle.TextColor : HudStyle.TextSecondaryColor, EHudTextAlign::Left);

	float Left = 0.f;
	float Width = 0.f;
	GetTrack(Size, Left, Width);
	const float TrackTop = (Size.Y - HubSlider::TrackHeight) * 0.5f;
	const float Filled = Width * Value;
	FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f(Left, TrackTop), FVector2f(Width, HubSlider::TrackHeight), HudStyle.EmptySegmentColor);
	FHudDraw::Box(OutDrawElements, Layer + 1, AllottedGeometry, FVector2f(Left, TrackTop), FVector2f(Filled, HubSlider::TrackHeight),
		FHudDraw::WithOpacity(HudStyle.PlayerColor, bActive ? 1.f : 0.75f));

	const FVector2f ThumbPosition(Left + Filled - HubSlider::ThumbWidth * 0.5f, (Size.Y - HubSlider::ThumbHeight) * 0.5f);
	FHudDraw::Box(OutDrawElements, Layer + 2, AllottedGeometry, ThumbPosition, FVector2f(HubSlider::ThumbWidth, HubSlider::ThumbHeight),
		bActive ? HudStyle.TextColor : HudStyle.PlayerColor);

	FHudDraw::Text(OutDrawElements, Layer + 2, AllottedGeometry, GetValueText(), Font, FVector2f(Size.X, TextTop),
		HudStyle.TextColor, EHudTextAlign::Right);
	return Layer + 2;
}

void UHubSliderRowWidget::SetFromLocalX(const FGeometry& Geometry, const float LocalX)
{
	float Left = 0.f;
	float Width = 0.f;
	GetTrack(FVector2f(Geometry.GetLocalSize()), Left, Width);
	const float Before = Value;
	SetValue((LocalX - Left) / Width);
	if (Value != Before)
	{
		OnValueChanged.Broadcast(Value);
	}
}

void UHubSliderRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHubSliderRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply UHubSliderRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	float Left = 0.f;
	float Width = 0.f;
	GetTrack(FVector2f(InGeometry.GetLocalSize()), Left, Width);
	const float LocalX = static_cast<float>(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).X);
	if (LocalX < Left - HubSlider::ThumbWidth || LocalX > Left + Width + HubSlider::ThumbWidth)
	{
		return FReply::Unhandled();
	}
	bDragging = true;
	SetFromLocalX(InGeometry, LocalX);
	return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FReply UHubSliderRowWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}
	SetFromLocalX(InGeometry, static_cast<float>(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).X));
	return FReply::Handled();
}

FReply UHubSliderRowWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	EndDrag();
	return FReply::Handled().ReleaseMouseCapture();
}

void UHubSliderRowWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	EndDrag();
}

void UHubSliderRowWidget::EndDrag()
{
	if (!bDragging)
	{
		return;
	}
	bDragging = false;
	Invalidate(EInvalidateWidgetReason::Paint);
	OnValueCommitted.Broadcast(Value);
}

FReply UHubSliderRowWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const float Step = Key == EKeys::Left ? -HubSlider::KeyStep : Key == EKeys::Right ? HubSlider::KeyStep : 0.f;
	if (Step == 0.f)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	const float Before = Value;
	SetValue(Value + Step);
	if (Value != Before)
	{
		OnValueChanged.Broadcast(Value);
		OnValueCommitted.Broadcast(Value);
	}
	return FReply::Handled();
}
