#include "UI/Controls/ControlsKeyCapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubPanelWidget.h"

namespace ControlsKeyCap
{
	constexpr float Height = 26.f;
	constexpr float MinWidth = 30.f;
	constexpr float TextPadding = 9.f;
	constexpr float BorderOpacity = 0.85f;
	constexpr float DimmedOpacity = 0.55f;
}

void UControlsKeyCapWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	USizeBox* const Root = WidgetTree->ConstructWidget<USizeBox>();
	Root->SetHeightOverride(ControlsKeyCap::Height);
	Root->SetMinDesiredWidth(ControlsKeyCap::MinWidth);
	WidgetTree->RootWidget = Root;

	UOverlay* const Cap = WidgetTree->ConstructWidget<UOverlay>();
	Root->AddChild(Cap);

	Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	Panel->BorderOpacity = ControlsKeyCap::BorderOpacity;
	UOverlaySlot* const PanelSlot = Cap->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	Text = MakeText(FText::GetEmpty(), HudStyle.ActionTextFontSize, HudStyle.TextColor, true);
	UOverlaySlot* const TextSlot = Cap->AddChildToOverlay(Text);
	TextSlot->SetHorizontalAlignment(HAlign_Center);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(ControlsKeyCap::TextPadding, 0.f));

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UControlsKeyCapWidget::Setup(const FText& Label, const bool bDimmed)
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	Text->SetText(Label);
	StyleText(*Text, HudStyle.ActionTextFontSize, bDimmed ? HudStyle.TextSecondaryColor : HudStyle.TextColor, !bDimmed);
	SetRenderOpacity(bDimmed ? ControlsKeyCap::DimmedOpacity : 1.f);
}

FText UControlsKeyCapWidget::GetLabel() const
{
	return Text ? Text->GetText() : FText::GetEmpty();
}
