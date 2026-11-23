#include "UI/Pause/PauseMenuPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubPanelWidget.h"

#define LOCTEXT_NAMESPACE "PauseMenuPanel"

namespace PauseMenuPanel
{
	constexpr float Width = 320.f;
	constexpr float Inset = 28.f;
	constexpr int32 TitleSize = 22;
	constexpr float ButtonGap = 14.f;
}

void UPauseMenuPanel::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	USizeBox* const Root = WidgetTree->ConstructWidget<USizeBox>();
	Root->SetWidthOverride(PauseMenuPanel::Width);
	WidgetTree->RootWidget = Root;

	UOverlay* const Card = WidgetTree->ConstructWidget<UOverlay>();
	Root->AddChild(Card);
	UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	UOverlaySlot* const PanelSlot = Card->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* const ColumnSlot = Card->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetPadding(FMargin(PauseMenuPanel::Inset));

	UTextBlock* const Title = MakeText(LOCTEXT("Title", "Paused"), PauseMenuPanel::TitleSize, HudStyle.PlayerColor, true);
	UVerticalBoxSlot* const TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, PauseMenuPanel::Inset));

	const auto AddButton = [this, Column](const FText& Label)
	{
		UHubButtonWidget* const Button = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
		Button->SetLabel(Label);
		UVerticalBoxSlot* const ButtonSlot = Column->AddChildToVerticalBox(Button);
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetPadding(FMargin(0.f, 0.f, 0.f, PauseMenuPanel::ButtonGap));
		return Button;
	};
	ResumeButton = AddButton(LOCTEXT("Resume", "Resume"));
	SettingsButton = AddButton(LOCTEXT("Settings", "Settings"));
	ControlsButton = AddButton(LOCTEXT("Controls", "Controls"));
	MainMenuButton = AddButton(LOCTEXT("MainMenu", "Quit to main menu"));
	QuitButton = AddButton(LOCTEXT("Quit", "Quit game"));
}

void UPauseMenuPanel::NativeConstruct()
{
	Super::NativeConstruct();
	ResumeButton->OnClicked.AddLambda([this]() { OnResume.Broadcast(); });
	SettingsButton->OnClicked.AddLambda([this]() { OnSettings.Broadcast(); });
	ControlsButton->OnClicked.AddLambda([this]() { OnControls.Broadcast(); });
	MainMenuButton->OnClicked.AddLambda([this]() { OnQuitToMainMenu.Broadcast(); });
	QuitButton->OnClicked.AddLambda([this]() { OnQuitGame.Broadcast(); });
}

void UPauseMenuPanel::NativeDestruct()
{
	ResumeButton->OnClicked.Clear();
	SettingsButton->OnClicked.Clear();
	ControlsButton->OnClicked.Clear();
	MainMenuButton->OnClicked.Clear();
	QuitButton->OnClicked.Clear();
	Super::NativeDestruct();
}

#undef LOCTEXT_NAMESPACE
