#include "UI/Menu/MainMenuCallsignPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Flow/CallsignRules.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"

#define LOCTEXT_NAMESPACE "MainMenuCallsign"

namespace MainMenuCallsign
{
	constexpr float Width = 420.f;
	constexpr int32 HeadingSize = 22;
	constexpr int32 FieldFontSize = 18;
	constexpr float Gap = 14.f;
}

void UMainMenuCallsignPanel::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	USizeBox* const Root = WidgetTree->ConstructWidget<USizeBox>();
	Root->SetWidthOverride(MainMenuCallsign::Width);
	WidgetTree->RootWidget = Root;

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->AddChild(Column);

	HeadingText = MakeText(LOCTEXT("Heading", "Callsign"), MainMenuCallsign::HeadingSize, HudStyle.PlayerColor, true);
	Column->AddChildToVerticalBox(HeadingText);
	SlotText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	Column->AddChildToVerticalBox(SlotText)->SetPadding(FMargin(0.f, 2.f, 0.f, MainMenuCallsign::Gap));

	CallsignField = WidgetTree->ConstructWidget<UEditableTextBox>();
	FEditableTextBoxStyle FieldStyle = CallsignField->GetWidgetStyle();
	FieldStyle.SetFont(UStartechHudStyle::MakeFont(MainMenuCallsign::FieldFontSize));
	FieldStyle.SetForegroundColor(FSlateColor(HudStyle.TextColor));
	FieldStyle.SetFocusedForegroundColor(FSlateColor(HudStyle.TextColor));
	FieldStyle.SetBackgroundColor(FSlateColor(HudStyle.PanelFillColor));
	FieldStyle.SetPadding(FMargin(10.f, 8.f));
	CallsignField->SetWidgetStyle(FieldStyle);
	CallsignField->SetHintText(LOCTEXT("Hint", "Enter a callsign"));
	Column->AddChildToVerticalBox(CallsignField);

	ErrorText = MakeText(LOCTEXT("Error", "Use 3–16 letters, digits or spaces"), HudStyle.SmallFontSize, HudStyle.DangerTextColor);
	Column->AddChildToVerticalBox(ErrorText)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.f, MainMenuCallsign::Gap * 1.5f, 0.f, 0.f));

	BackButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	BackButton->SetQuiet(true);
	BackButton->SetLabel(LOCTEXT("Back", "Back"));
	UHorizontalBoxSlot* const BackSlot = Buttons->AddChildToHorizontalBox(BackButton);
	BackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BackSlot->SetHorizontalAlignment(HAlign_Left);
	BackSlot->SetVerticalAlignment(VAlign_Center);

	StartButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	StartButton->SetLabel(LOCTEXT("Start", "Start"));
	Buttons->AddChildToHorizontalBox(StartButton)->SetVerticalAlignment(VAlign_Center);
}

void UMainMenuCallsignPanel::NativeConstruct()
{
	Super::NativeConstruct();
	CallsignField->OnTextChanged.AddUniqueDynamic(this, &UMainMenuCallsignPanel::HandleTextChanged);
	CallsignField->OnTextCommitted.AddUniqueDynamic(this, &UMainMenuCallsignPanel::HandleTextCommitted);
	StartButton->OnClicked.AddUObject(this, &UMainMenuCallsignPanel::TryStart);
	BackButton->OnClicked.AddLambda([this]() { OnBack.Broadcast(); });
	Validate();
}

void UMainMenuCallsignPanel::NativeDestruct()
{
	CallsignField->OnTextChanged.RemoveDynamic(this, &UMainMenuCallsignPanel::HandleTextChanged);
	CallsignField->OnTextCommitted.RemoveDynamic(this, &UMainMenuCallsignPanel::HandleTextCommitted);
	StartButton->OnClicked.RemoveAll(this);
	BackButton->OnClicked.Clear();
	Super::NativeDestruct();
}

void UMainMenuCallsignPanel::Prepare(const int32 SlotIndex)
{
	SlotText->SetText(FText::Format(LOCTEXT("Slot", "Slot {0}"), FText::AsNumber(SlotIndex + 1)));
	CallsignField->SetText(FText::GetEmpty());
	Validate();
	CallsignField->SetKeyboardFocus();
}

void UMainMenuCallsignPanel::HandleTextChanged(const FText& Text)
{
	Validate();
}

void UMainMenuCallsignPanel::HandleTextCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		TryStart();
	}
}

void UMainMenuCallsignPanel::Validate()
{
	const bool bValid = FCallsignRules::IsValid(GetCallsign());
	ErrorText->SetVisibility(bValid ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	StartButton->SetButtonEnabled(bValid);
}

void UMainMenuCallsignPanel::TryStart()
{
	const FString Callsign = GetCallsign();
	if (FCallsignRules::IsValid(Callsign))
	{
		OnStart.Broadcast(Callsign);
	}
}

FString UMainMenuCallsignPanel::GetCallsign() const
{
	return FCallsignRules::Normalize(CallsignField->GetText().ToString());
}

#undef LOCTEXT_NAMESPACE
