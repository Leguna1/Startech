#include "UI/Menu/MainMenuSlotsPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Menu/MainMenuSlotCard.h"

#define LOCTEXT_NAMESPACE "MainMenuSlots"

namespace MainMenuSlots
{
	constexpr float Width = 560.f;
	constexpr float CardGap = 10.f;
	constexpr float SectionGap = 20.f;
	constexpr int32 HeadingSize = 22;
}

void UMainMenuSlotsPanel::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	USizeBox* const Root = WidgetTree->ConstructWidget<USizeBox>();
	Root->SetWidthOverride(MainMenuSlots::Width);
	WidgetTree->RootWidget = Root;

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Root->AddChild(Column);

	HeadingText = MakeText(FText::GetEmpty(), MainMenuSlots::HeadingSize, HudStyle.PlayerColor, true);
	Column->AddChildToVerticalBox(HeadingText)->SetPadding(FMargin(0.f, 0.f, 0.f, MainMenuSlots::SectionGap));

	CardsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(CardsBox);

	StatusText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.DangerTextColor);
	StatusText->SetAutoWrapText(true);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, MainMenuSlots::CardGap, 0.f, 0.f));

	BackButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	BackButton->SetQuiet(true);
	BackButton->SetLabel(LOCTEXT("Back", "Back"));
	UVerticalBoxSlot* const BackSlot = Column->AddChildToVerticalBox(BackButton);
	BackSlot->SetHorizontalAlignment(HAlign_Left);
	BackSlot->SetPadding(FMargin(0.f, MainMenuSlots::SectionGap, 0.f, 0.f));
}

void UMainMenuSlotsPanel::NativeConstruct()
{
	Super::NativeConstruct();
	BackButton->OnClicked.AddLambda([this]() { OnBack.Broadcast(); });
}

void UMainMenuSlotsPanel::NativeDestruct()
{
	BackButton->OnClicked.Clear();
	ClearCards();
	Super::NativeDestruct();
}

void UMainMenuSlotsPanel::Show(const EMainMenuSlotMode Mode, const TArray<FSaveSlotInfo>& Infos)
{
	const bool bLoadMode = Mode == EMainMenuSlotMode::Load;
	HeadingText->SetText(bLoadMode ? LOCTEXT("LoadHeading", "Load") : LOCTEXT("NewHeading", "New game: choose a slot"));
	SetStatus(FText::GetEmpty());
	ClearCards();

	for (const FSaveSlotInfo& Info : Infos)
	{
		if (bLoadMode && !Info.bOccupied)
		{
			continue;
		}
		UMainMenuSlotCard* const Card = WidgetTree->ConstructWidget<UMainMenuSlotCard>(UMainMenuSlotCard::StaticClass());
		Card->Setup(Info, bLoadMode);
		Card->OnPrimary.AddLambda([this](const int32 SlotIndex) { OnSlotPicked.Broadcast(SlotIndex); });
		Card->OnDelete.AddLambda([this](const int32 SlotIndex) { OnDeleteRequested.Broadcast(SlotIndex); });
		CardsBox->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 0.f, 0.f, MainMenuSlots::CardGap));
		Cards.Add(Card);
	}
}

void UMainMenuSlotsPanel::SetStatus(const FText& Message)
{
	StatusText->SetText(Message);
	StatusText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMainMenuSlotsPanel::ClearCards()
{
	for (UMainMenuSlotCard* const Card : Cards)
	{
		if (Card)
		{
			Card->OnPrimary.Clear();
			Card->OnDelete.Clear();
		}
	}
	Cards.Reset();
	if (CardsBox)
	{
		CardsBox->ClearChildren();
	}
}

#undef LOCTEXT_NAMESPACE
