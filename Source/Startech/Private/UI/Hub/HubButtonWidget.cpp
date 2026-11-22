#include "UI/Hub/HubButtonWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace HubButton
{
	/** Narrowest panel button, so short labels still make a clear target. */
	constexpr float MinMainWidth = 96.f;
	/** Narrowest Toggle button: quantity chips are short. */
	constexpr float MinToggleWidth = 44.f;
	/** Padding either side of a quiet button's label. */
	constexpr float QuietPadding = 6.f;
	/** Padding either side of a Toggle's label. */
	constexpr float TogglePadding = 10.f;
	/** Space between the label and the second line. */
	constexpr float LineGap = 1.f;
	/** Between the label and the accent. */
	const TCHAR* AccentSeparator = TEXT(" · ");
}

UHubButtonWidget::UHubButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
}

void UHubButtonWidget::BuildContent()
{
	SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	WidgetTree->RootWidget = SizeBox;
	UpdateSize();
}

void UHubButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const UStartechHudStyle& HudStyle = GetHudStyle();
	Panel.InitializeWithFill(this, HudStyle, HudStyle.ButtonFillColor, HudStyle.PlayerColor);
	UpdateSize();
	UpdateState();
}

void UHubButtonWidget::SetLabel(const FText& InLabel)
{
	if (Label.IdenticalTo(InLabel) || Label.EqualTo(InLabel))
	{
		return;
	}
	Label = InLabel;
	UpdateSize();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHubButtonWidget::SetAccent(const FText& InAccent, const FLinearColor& InColor)
{
	if (Accent.EqualTo(InAccent) && AccentColor.Equals(InColor))
	{
		return;
	}
	Accent = InAccent;
	AccentColor = InColor;
	UpdateSize();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHubButtonWidget::SetSecondLine(const FText& InSecondLine, const FLinearColor& InColor)
{
	if (SecondLine.EqualTo(InSecondLine) && SecondLineColor.Equals(InColor))
	{
		return;
	}
	SecondLine = InSecondLine;
	SecondLineColor = InColor;
	UpdateSize();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHubButtonWidget::SetButtonEnabled(const bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}
	bEnabled = bInEnabled;
	bPressed = false;
	UpdateState();
}

void UHubButtonWidget::SetVariant(const EHubButtonVariant InVariant)
{
	if (Variant == InVariant)
	{
		return;
	}
	Variant = InVariant;
	UpdateSize();
	UpdateState();
}

void UHubButtonWidget::SetSelected(const bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}
	bSelected = bInSelected;
	UpdateState();
}

void UHubButtonWidget::SetFixedWidth(const float InWidth)
{
	FixedWidth = FMath::Max(InWidth, 0.f);
	UpdateSize();
}

FString UHubButtonWidget::GetFirstLine() const
{
	return Accent.IsEmpty() ? Label.ToString() : Label.ToString() + HubButton::AccentSeparator + Accent.ToString();
}

void UHubButtonWidget::UpdateSize()
{
	if (!SizeBox)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, Variant != EHubButtonVariant::Quiet);
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const FVector2f SecondSize = SecondLine.IsEmpty() ? FVector2f::ZeroVector : FHudDraw::Measure(SecondLine.ToString(), SmallFont);
	const float TextWidth = FMath::Max(FHudDraw::Measure(GetFirstLine(), Font).X, SecondSize.X);

	float Width = 0.f;
	switch (Variant)
	{
	case EHubButtonVariant::Quiet:
		Width = TextWidth + 2.f * HubButton::QuietPadding;
		break;
	case EHubButtonVariant::Toggle:
		Width = FMath::Max(TextWidth + 2.f * HubButton::TogglePadding, HubButton::MinToggleWidth);
		break;
	default:
		Width = FMath::Max(TextWidth + 2.f * HudStyle.HubButtonPadding, HubButton::MinMainWidth);
		break;
	}

	SizeBox->SetWidthOverride(FixedWidth > 0.f ? FixedWidth : Width);
	SizeBox->SetHeightOverride(HudStyle.HubButtonHeight + (SecondLine.IsEmpty() ? 0.f : SecondSize.Y + HubButton::LineGap));
}

void UHubButtonWidget::UpdateState()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const bool bActive = bEnabled && bHovered;
	const bool bPrimaryLook = Variant == EHubButtonVariant::Primary
		|| (Variant == EHubButtonVariant::Toggle && bSelected);

	FLinearColor Fill;
	if (Variant == EHubButtonVariant::Toggle && bSelected)
	{
		// A selected chip stays bright; hovering it changes nothing, since clicking it again does nothing.
		Fill = HudStyle.ButtonSelectedFillColor;
	}
	else if (bPrimaryLook)
	{
		Fill = bActive && bPressed ? HudStyle.ButtonPressedFillColor
			: bActive ? HudStyle.ButtonHoverFillColor : HudStyle.ButtonFillColor;
	}
	else
	{
		Fill = bActive && bPressed ? HudStyle.ButtonHoverFillColor
			: bActive ? HudStyle.ButtonFillColor : HudStyle.SecondaryButtonFillColor;
	}

	Panel.SetFillColor(Fill);
	Panel.SetBorderColor(bPrimaryLook || bActive ? HudStyle.PlayerColor : HudStyle.SecondaryButtonBorderColor);
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UHubButtonWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Opacity = bEnabled ? 1.f : HudStyle.ButtonDisabledOpacity;
	const bool bQuiet = Variant == EHubButtonVariant::Quiet;
	const bool bBold = !bQuiet && (Variant != EHubButtonVariant::Toggle || bSelected);
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, bBold);
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const FString First = GetFirstLine();
	const float TextHeight = FHudDraw::Measure(First, Font).Y;
	const float SecondHeight = SecondLine.IsEmpty() ? 0.f : FHudDraw::Measure(SecondLine.ToString(), SmallFont).Y;
	const float BlockHeight = TextHeight + (SecondLine.IsEmpty() ? 0.f : HubButton::LineGap + SecondHeight);
	const float Top = (Size.Y - BlockHeight) * 0.5f;

	FLinearColor TextColor = HudStyle.TextColor;
	if (bQuiet)
	{
		TextColor = bEnabled && bHovered ? HudStyle.TextColor : HudStyle.TextSecondaryColor;
	}
	else
	{
		Panel.DrawWithOpacity(OutDrawElements, Layer, AllottedGeometry, FVector2f::ZeroVector, Size, HudStyle, Opacity);
		if (Variant == EHubButtonVariant::Toggle && !bSelected && !bHovered)
		{
			TextColor = HudStyle.TextSecondaryColor;
		}
	}

	if (Accent.IsEmpty())
	{
		FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, First, Font, FVector2f(Size.X * 0.5f, Top),
			FHudDraw::WithOpacity(TextColor, Opacity), EHudTextAlign::Centre);
	}
	else
	{
		// Placed from the first part's own width, so the space after the separator is kept.
		const FString FirstPart = Label.ToString() + HubButton::AccentSeparator;
		const FString SecondPart = Accent.ToString();
		const float FirstWidth = FHudDraw::Measure(FirstPart, Font).X;
		const float Left = (Size.X - FirstWidth - FHudDraw::Measure(SecondPart, Font).X) * 0.5f;
		FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, FirstPart, Font, FVector2f(Left, Top),
			FHudDraw::WithOpacity(TextColor, Opacity), EHudTextAlign::Left);
		FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, SecondPart, Font, FVector2f(Left + FirstWidth, Top),
			FHudDraw::WithOpacity(AccentColor, Opacity), EHudTextAlign::Left);
	}

	if (!SecondLine.IsEmpty())
	{
		FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, SecondLine.ToString(), SmallFont,
			FVector2f(Size.X * 0.5f, Top + TextHeight + HubButton::LineGap),
			FHudDraw::WithOpacity(SecondLineColor, Opacity), EHudTextAlign::Centre);
	}
	return Layer + 1;
}

void UHubButtonWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	UpdateState();
}

void UHubButtonWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	UpdateState();
}

FReply UHubButtonWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bEnabled || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}

	bPressed = true;
	UpdateState();
	const TSharedPtr<SWidget> Slate = GetCachedWidget();
	return Slate.IsValid() ? FReply::Handled().CaptureMouse(Slate.ToSharedRef()) : FReply::Handled();
}

FReply UHubButtonWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bPressed || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}

	bPressed = false;
	const bool bOver = FHudDraw::IsInside(FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())),
		FVector2f(InGeometry.GetLocalSize()));
	bHovered = bOver;
	UpdateState();

	if (bOver && bEnabled)
	{
		LastClickModifiers = InMouseEvent.GetModifierKeys();

		// Last: a listener may close the hub, which destroys this widget.
		OnClicked.Broadcast();
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void UHubButtonWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
	bPressed = false;
	UpdateState();
}
