#include "UI/Controls/ControlsScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Controls/ControlsHelpData.h"
#include "Controls/ControlsList.h"
#include "UI/Controls/ControlsRowWidget.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubPanelWidget.h"

#define LOCTEXT_NAMESPACE "ControlsScreen"

namespace ControlsScreen
{
	constexpr float Width = 960.f;
	constexpr float MaxHeight = 780.f;
	constexpr float Inset = 28.f;
	constexpr int32 TitleSize = 22;
	constexpr float TitleGap = 16.f;
	constexpr float TipsInset = 14.f;
	constexpr float TipsGap = 16.f;
	constexpr float TipLineGap = 3.f;
	constexpr float GroupGap = 14.f;
	constexpr float HeadingGap = 4.f;
	constexpr float BackGap = 16.f;
	constexpr float BackWidth = 160.f;
	constexpr float TipsBorderOpacity = 0.6f;
}

void UControlsScreenWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	USizeBox* const Frame = WidgetTree->ConstructWidget<USizeBox>();
	Frame->SetWidthOverride(ControlsScreen::Width);
	Frame->SetMaxDesiredHeight(ControlsScreen::MaxHeight);
	UOverlaySlot* const FrameSlot = Root->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Center);
	FrameSlot->SetVerticalAlignment(VAlign_Center);

	UOverlay* const Card = WidgetTree->ConstructWidget<UOverlay>();
	Frame->AddChild(Card);
	UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	UOverlaySlot* const PanelSlot = Card->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* const ColumnSlot = Card->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	ColumnSlot->SetPadding(FMargin(ControlsScreen::Inset));

	UTextBlock* const Title = MakeText(LOCTEXT("Title", "Controls"), ControlsScreen::TitleSize, HudStyle.PlayerColor, true);
	UVerticalBoxSlot* const TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, ControlsScreen::TitleGap));

	// The tips box: its own cut-corner panel, brighter outline.
	UOverlay* const Tips = WidgetTree->ConstructWidget<UOverlay>();
	Column->AddChildToVerticalBox(Tips)->SetPadding(FMargin(0.f, 0.f, 0.f, ControlsScreen::TipsGap));
	UHubPanelWidget* const TipsPanel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	TipsPanel->BorderOpacity = ControlsScreen::TipsBorderOpacity;
	UOverlaySlot* const TipsPanelSlot = Tips->AddChildToOverlay(TipsPanel);
	TipsPanelSlot->SetHorizontalAlignment(HAlign_Fill);
	TipsPanelSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const TipsColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* const TipsColumnSlot = Tips->AddChildToOverlay(TipsColumn);
	TipsColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	TipsColumnSlot->SetPadding(FMargin(ControlsScreen::TipsInset));
	TipsTitle = MakeText(FText::GetEmpty(), HudStyle.HeadingFontSize, HudStyle.CreditsColor, true);
	TipsColumn->AddChildToVerticalBox(TipsTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, ControlsScreen::HeadingGap));
	TipsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	TipsColumn->AddChildToVerticalBox(TipsBox);

	Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* const ScrollSlot = Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(ListBox);

	BackButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	BackButton->SetLabel(LOCTEXT("Back", "Back"));
	BackButton->SetVariant(EHubButtonVariant::Secondary);
	BackButton->SetFixedWidth(ControlsScreen::BackWidth);
	UVerticalBoxSlot* const BackSlot = Column->AddChildToVerticalBox(BackButton);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0.f, ControlsScreen::BackGap, 0.f, 0.f));
}

void UControlsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BackButton->OnClicked.AddUObject(this, &UControlsScreenWidget::Close);
}

void UControlsScreenWidget::NativeDestruct()
{
	BackButton->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void UControlsScreenWidget::OnPageOpened()
{
	Rebuild();
	Scroll->ScrollToStart();
}

void UControlsScreenWidget::Rebuild()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	TipsBox->ClearChildren();
	ListBox->ClearChildren();
	Rows.Reset();
	RowGroups.Reset();

	const UControlsHelpData* const Data = UControlsHelpData::Load();
	if (!Data)
	{
		TipsTitle->SetText(FText::GetEmpty());
		return;
	}

	TipsTitle->SetText(Data->TipsTitle);
	for (const FText& Tip : Data->Tips)
	{
		UTextBlock* const Line = MakeText(FText::Format(LOCTEXT("TipLine", "•  {0}"), FControlsList::FormatWithKeys(*Data, Tip)), HudStyle.BodyFontSize, HudStyle.TextColor);
		Line->SetAutoWrapText(true);
		TipsBox->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, ControlsScreen::TipLineGap, 0.f, 0.f));
	}

	bool bFirst = true;
	for (const FControlsListGroup& Group : FControlsList::Build(*Data))
	{
		UTextBlock* const Heading = MakeText(Group.DisplayName, HudStyle.HeadingFontSize, HudStyle.PlayerColor, true);
		ListBox->AddChildToVerticalBox(Heading)->SetPadding(
			FMargin(0.f, bFirst ? 0.f : ControlsScreen::GroupGap, 0.f, ControlsScreen::HeadingGap));
		bFirst = false;

		for (const FControlsListRow& Row : Group.Rows)
		{
			UControlsRowWidget* const RowWidget = WidgetTree->ConstructWidget<UControlsRowWidget>(UControlsRowWidget::StaticClass());
			RowWidget->Setup(Row);
			ListBox->AddChildToVerticalBox(RowWidget);
			Rows.Add(RowWidget);
			RowGroups.Add(Group.DisplayName);
		}
	}
}

TArray<FText> UControlsScreenWidget::GetTipLines() const
{
	TArray<FText> Lines;
	for (UWidget* const Child : TipsBox->GetAllChildren())
	{
		if (const UTextBlock* const Line = Cast<UTextBlock>(Child))
		{
			Lines.Add(Line->GetText());
		}
	}
	return Lines;
}

#undef LOCTEXT_NAMESPACE
