#include "ShopQuantityBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"

#define LOCTEXT_NAMESPACE "ShopQuantityBar"

namespace ShopQuantityBar
{
	constexpr float ChipGap = 6.f;
	constexpr float ChipWidth = 64.f;
	constexpr float TopGap = 8.f;
	constexpr float StatusGap = 6.f;
}

void UShopQuantityBarWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UVerticalBox* const Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;

	ChipRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChildToVerticalBox(ChipRow)->SetPadding(FMargin(0.f, ShopQuantityBar::TopGap, 0.f, 0.f));

	for (const EShopQuantity Quantity : FShopQuantity::All())
	{
		UHubButtonWidget* const Chip = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
		Chip->SetVariant(EHubButtonVariant::Toggle);
		Chip->SetFixedWidth(ShopQuantityBar::ChipWidth);
		Chip->SetLabel(FShopQuantity::GetLabel(Quantity));
		Chip->SetSelected(Quantity == Selected);
		ChipRow->AddChildToHorizontalBox(Chip)->SetPadding(FMargin(0.f, 0.f, ShopQuantityBar::ChipGap, 0.f));
		Chips.Add(Chip);
	}

	StatusText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	Root->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(2.f, ShopQuantityBar::StatusGap, 0.f, ShopQuantityBar::StatusGap));
}

void UShopQuantityBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	for (int32 Index = 0; Index < Chips.Num(); ++Index)
	{
		Chips[Index]->OnClicked.AddUObject(this, &UShopQuantityBarWidget::HandleChipClicked, FShopQuantity::All()[Index]);
	}
}

void UShopQuantityBarWidget::NativeDestruct()
{
	for (UHubButtonWidget* const Chip : Chips)
	{
		Chip->OnClicked.RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UShopQuantityBarWidget::SetQuantity(const EShopQuantity Quantity)
{
	Selected = Quantity;
	for (int32 Index = 0; Index < Chips.Num(); ++Index)
	{
		Chips[Index]->SetSelected(FShopQuantity::All()[Index] == Selected);
	}
}

void UShopQuantityBarWidget::SetChipsVisible(const bool bVisible)
{
	ChipRow->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UShopQuantityBarWidget::SetStatus(const FText& Text, const FLinearColor& Color)
{
	StatusText->SetText(Text);
	StatusText->SetColorAndOpacity(FSlateColor(Color));
}

void UShopQuantityBarWidget::HandleChipClicked(const EShopQuantity Quantity)
{
	if (Quantity == Selected)
	{
		return;
	}
	SetQuantity(Quantity);
	OnQuantityChanged.Broadcast(Quantity);
}

#undef LOCTEXT_NAMESPACE
