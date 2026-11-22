#include "UI/Menu/MainMenuDialog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubPanelWidget.h"

#define LOCTEXT_NAMESPACE "MainMenuDialog"

namespace MainMenuDialog
{
	constexpr float Width = 460.f;
	constexpr float Inset = 24.f;
	constexpr float ButtonGap = 16.f;
}

void UMainMenuDialog::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UHubPanelWidget* const Dim = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	Dim->bBackdrop = true;
	UOverlaySlot* const DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* const Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(MainMenuDialog::Width);
	UOverlaySlot* const BoxSlot = Root->AddChildToOverlay(Box);
	BoxSlot->SetHorizontalAlignment(HAlign_Center);
	BoxSlot->SetVerticalAlignment(VAlign_Center);

	UOverlay* const Card = WidgetTree->ConstructWidget<UOverlay>();
	Box->AddChild(Card);
	UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	UOverlaySlot* const PanelSlot = Card->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* const ColumnSlot = Card->AddChildToOverlay(Column);
	ColumnSlot->SetPadding(FMargin(MainMenuDialog::Inset));
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);

	MessageText = MakeText(FText::GetEmpty(), HudStyle.HeadingFontSize, HudStyle.TextColor);
	MessageText->SetAutoWrapText(true);
	UVerticalBoxSlot* const MessageSlot = Column->AddChildToVerticalBox(MessageText);
	MessageSlot->SetPadding(FMargin(0.f, 0.f, 0.f, MainMenuDialog::Inset));

	UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* const ButtonsSlot = Column->AddChildToVerticalBox(Buttons);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Right);

	CancelButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	CancelButton->SetQuiet(true);
	CancelButton->SetLabel(LOCTEXT("Cancel", "Cancel"));
	UHorizontalBoxSlot* const CancelSlot = Buttons->AddChildToHorizontalBox(CancelButton);
	CancelSlot->SetVerticalAlignment(VAlign_Center);
	CancelSlot->SetPadding(FMargin(0.f, 0.f, MainMenuDialog::ButtonGap, 0.f));

	ConfirmButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	ConfirmButton->SetLabel(LOCTEXT("Ok", "OK"));
	Buttons->AddChildToHorizontalBox(ConfirmButton)->SetVerticalAlignment(VAlign_Center);
}

void UMainMenuDialog::NativeConstruct()
{
	Super::NativeConstruct();
	ConfirmButton->OnClicked.AddUObject(this, &UMainMenuDialog::HandleConfirmClicked);
	CancelButton->OnClicked.AddUObject(this, &UMainMenuDialog::Cancel);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UMainMenuDialog::NativeDestruct()
{
	ConfirmButton->OnClicked.RemoveAll(this);
	CancelButton->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void UMainMenuDialog::Ask(const FText& Message, const FText& ConfirmLabel, TFunction<void()> OnConfirm)
{
	PendingAction = MoveTemp(OnConfirm);
	Open(Message, ConfirmLabel, true);
}

void UMainMenuDialog::ShowMessage(const FText& Message)
{
	PendingAction = nullptr;
	Open(Message, LOCTEXT("Ok", "OK"), false);
}

void UMainMenuDialog::Cancel()
{
	PendingAction = nullptr;
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UMainMenuDialog::IsOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

void UMainMenuDialog::Open(const FText& Message, const FText& ConfirmLabel, const bool bShowCancel)
{
	MessageText->SetText(Message);
	ConfirmButton->SetLabel(ConfirmLabel);
	CancelButton->SetVisibility(bShowCancel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Visible);
}

void UMainMenuDialog::HandleConfirmClicked()
{
	// Closed first: the action may open the box again (or leave the menu).
	const TFunction<void()> Action = MoveTemp(PendingAction);
	PendingAction = nullptr;
	SetVisibility(ESlateVisibility::Collapsed);
	if (Action)
	{
		Action();
	}
}

#undef LOCTEXT_NAMESPACE
