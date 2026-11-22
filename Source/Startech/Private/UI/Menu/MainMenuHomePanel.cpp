#include "UI/Menu/MainMenuHomePanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"

#define LOCTEXT_NAMESPACE "MainMenuHome"

namespace MainMenuHome
{
	constexpr int32 TitleSize = 48;
	constexpr float TitleGap = 48.f;
	constexpr float ButtonGap = 14.f;
}

void UMainMenuHomePanel::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Column;

	UTextBlock* const Title = MakeText(LOCTEXT("Title", "Startech"), MainMenuHome::TitleSize, HudStyle.PlayerColor, true);
	UVerticalBoxSlot* const TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MainMenuHome::TitleGap));

	ContinueButton = AddButton(*Column, LOCTEXT("Continue", "Continue"));
	ContinueDetail = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	UVerticalBoxSlot* const DetailSlot = Column->AddChildToVerticalBox(ContinueDetail);
	DetailSlot->SetHorizontalAlignment(HAlign_Center);
	DetailSlot->SetPadding(FMargin(0.f, -MainMenuHome::ButtonGap * 0.5f, 0.f, MainMenuHome::ButtonGap));

	NewGameButton = AddButton(*Column, LOCTEXT("NewGame", "New game"));
	LoadButton = AddButton(*Column, LOCTEXT("Load", "Load"));
	QuitButton = AddButton(*Column, LOCTEXT("Quit", "Quit"));
}

UHubButtonWidget* UMainMenuHomePanel::AddButton(UVerticalBox& Column, const FText& Label)
{
	UHubButtonWidget* const Button = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	Button->SetLabel(Label);
	UVerticalBoxSlot* const ButtonSlot = Column.AddChildToVerticalBox(Button);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MainMenuHome::ButtonGap));
	return Button;
}

void UMainMenuHomePanel::NativeConstruct()
{
	Super::NativeConstruct();
	ContinueButton->OnClicked.AddLambda([this]() { OnContinue.Broadcast(); });
	NewGameButton->OnClicked.AddLambda([this]() { OnNewGame.Broadcast(); });
	LoadButton->OnClicked.AddLambda([this]() { OnLoad.Broadcast(); });
	QuitButton->OnClicked.AddLambda([this]() { OnQuit.Broadcast(); });
}

void UMainMenuHomePanel::NativeDestruct()
{
	ContinueButton->OnClicked.Clear();
	NewGameButton->OnClicked.Clear();
	LoadButton->OnClicked.Clear();
	QuitButton->OnClicked.Clear();
	Super::NativeDestruct();
}

void UMainMenuHomePanel::SetContinue(const bool bAvailable, const FText& Detail)
{
	ContinueButton->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ContinueDetail->SetVisibility(bAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ContinueDetail->SetText(Detail);
}

void UMainMenuHomePanel::SetLoadEnabled(const bool bEnabled)
{
	LoadButton->SetButtonEnabled(bEnabled);
}

#undef LOCTEXT_NAMESPACE
