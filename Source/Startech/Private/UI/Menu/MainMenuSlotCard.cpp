#include "UI/Menu/MainMenuSlotCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubPanelWidget.h"

#define LOCTEXT_NAMESPACE "MainMenuSlotCard"

namespace MainMenuSlotCard
{
	constexpr float Inset = 16.f;
	constexpr float LineGap = 3.f;
	constexpr float ButtonGap = 12.f;
}

FText UMainMenuSlotCard::FormatPlayTime(const double Seconds)
{
	const int64 Minutes = FMath::FloorToInt64(FMath::Max(Seconds, 0.0) / 60.0);
	FNumberFormattingOptions TwoDigits;
	TwoDigits.MinimumIntegralDigits = 2;
	return FText::Format(LOCTEXT("PlayTime", "{0}:{1}"), FText::AsNumber(Minutes / 60), FText::AsNumber(Minutes % 60, &TwoDigits));
}

void UMainMenuSlotCard::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	UOverlaySlot* const PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* const RowSlot = Root->AddChildToOverlay(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
	RowSlot->SetPadding(FMargin(MainMenuSlotCard::Inset));

	UVerticalBox* const Texts = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* const TextsSlot = Row->AddChildToHorizontalBox(Texts);
	TextsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextsSlot->SetVerticalAlignment(VAlign_Center);

	SlotText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	Texts->AddChildToVerticalBox(SlotText);
	NameText = MakeText(FText::GetEmpty(), HudStyle.HeadingFontSize, HudStyle.TextColor, true);
	Texts->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.f, MainMenuSlotCard::LineGap));
	DetailText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	Texts->AddChildToVerticalBox(DetailText);
	BackupText = MakeText(LOCTEXT("Restored", "Restored from backup"), HudStyle.SmallFontSize, HudStyle.HullColor);
	Texts->AddChildToVerticalBox(BackupText)->SetPadding(FMargin(0.f, MainMenuSlotCard::LineGap, 0.f, 0.f));

	DeleteButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	DeleteButton->SetQuiet(true);
	DeleteButton->SetLabel(LOCTEXT("Delete", "Delete"));
	UHorizontalBoxSlot* const DeleteSlot = Row->AddChildToHorizontalBox(DeleteButton);
	DeleteSlot->SetVerticalAlignment(VAlign_Center);
	DeleteSlot->SetPadding(FMargin(MainMenuSlotCard::ButtonGap, 0.f));

	PrimaryButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	UHorizontalBoxSlot* const PrimarySlot = Row->AddChildToHorizontalBox(PrimaryButton);
	PrimarySlot->SetVerticalAlignment(VAlign_Center);

	Apply();
}

void UMainMenuSlotCard::NativeConstruct()
{
	Super::NativeConstruct();
	PrimaryButton->OnClicked.AddLambda([this]() { OnPrimary.Broadcast(Info.Header.SlotIndex); });
	DeleteButton->OnClicked.AddLambda([this]() { OnDelete.Broadcast(Info.Header.SlotIndex); });
}

void UMainMenuSlotCard::NativeDestruct()
{
	PrimaryButton->OnClicked.Clear();
	DeleteButton->OnClicked.Clear();
	Super::NativeDestruct();
}

void UMainMenuSlotCard::Setup(const FSaveSlotInfo& InInfo, const bool bInLoadMode)
{
	Info = InInfo;
	bLoadMode = bInLoadMode;
	Apply();
}

void UMainMenuSlotCard::Apply()
{
	if (!NameText)
	{
		return; // Not built yet; BuildContent applies.
	}

	SlotText->SetText(FText::Format(LOCTEXT("Slot", "Slot {0}"), FText::AsNumber(Info.Header.SlotIndex + 1)));

	if (Info.bOccupied)
	{
		NameText->SetText(FText::FromString(Info.Header.DisplayName));
		DetailText->SetText(FText::Format(LOCTEXT("Detail", "Played {0}  ·  Saved {1}"), FormatPlayTime(Info.Header.PlayTimeSeconds),
			FText::AsDateTime(Info.Header.SavedUtc, EDateTimeStyle::Medium, EDateTimeStyle::Short)));
		DetailText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		NameText->SetText(LOCTEXT("Empty", "Empty"));
		DetailText->SetVisibility(ESlateVisibility::Collapsed);
	}
	BackupText->SetVisibility(Info.bOccupied && Info.bRestoredFromBackup ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	PrimaryButton->SetLabel(bLoadMode ? LOCTEXT("Load", "Load") : LOCTEXT("Select", "Select"));
	DeleteButton->SetVisibility(bLoadMode && Info.bOccupied ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
