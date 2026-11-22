#include "UI/Hub/HubPanelWidget.h"

#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

UHubPanelWidget::UHubPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHubPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const UStartechHudStyle& HudStyle = GetHudStyle();
	Panel.Initialize(this, HudStyle, FHudDraw::WithOpacity(HudStyle.PlayerColor, BorderOpacity));
}

int32 UHubPanelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	if (bBackdrop)
	{
		FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f::ZeroVector, Size, HudStyle.HubBackdropColor);
	}
	else
	{
		Panel.Draw(OutDrawElements, Layer, AllottedGeometry, FVector2f::ZeroVector, Size, HudStyle);
	}
	return Layer;
}
