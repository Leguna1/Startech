#include "UI/Hub/HubTopBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "Station.h"
#include "StartechShipPawn.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubPanelWidget.h"

#define LOCTEXT_NAMESPACE "HubTopBar"

namespace HubTopBar
{
	constexpr float SideInset = 20.f;
	constexpr float ItemGap = 28.f;
}

void UHubTopBarWidget::BuildContent()
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
	RowSlot->SetVerticalAlignment(VAlign_Center);
	RowSlot->SetPadding(FMargin(HubTopBar::SideInset, 0.f, HubTopBar::SideInset * 0.5f, 0.f));

	StationText = MakeText(FText::GetEmpty(), HudStyle.HeadingFontSize, HudStyle.PlayerColor, true);
	UHorizontalBoxSlot* const StationSlot = Row->AddChildToHorizontalBox(StationText);
	StationSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StationSlot->SetVerticalAlignment(VAlign_Center);

	CreditsText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.CreditsColor, true);
	UHorizontalBoxSlot* const CreditsSlot = Row->AddChildToHorizontalBox(CreditsText);
	CreditsSlot->SetVerticalAlignment(VAlign_Center);
	CreditsSlot->SetPadding(FMargin(0.f, 0.f, HubTopBar::ItemGap, 0.f));

	CargoText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.TextColor);
	UHorizontalBoxSlot* const CargoSlot = Row->AddChildToHorizontalBox(CargoText);
	CargoSlot->SetVerticalAlignment(VAlign_Center);
	CargoSlot->SetPadding(FMargin(0.f, 0.f, HubTopBar::ItemGap, 0.f));

	UndockButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	UndockButton->SetLabel(LOCTEXT("Undock", "Undock"));
	UHorizontalBoxSlot* const UndockSlot = Row->AddChildToHorizontalBox(UndockButton);
	UndockSlot->SetVerticalAlignment(VAlign_Center);
}

void UHubTopBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UndockButton->OnClicked.AddUObject(this, &UHubTopBarWidget::HandleUndockClicked);

	const AStartechShipPawn* const Ship = GetShip();
	const AStation* const Station = Ship ? Ship->GetDockedStation() : nullptr;
	StationText->SetText(Station ? Station->GetDisplayName() : FText::GetEmpty());

	UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr;
	BoundInventory = Inventory;
	if (Inventory)
	{
		Inventory->OnCreditsChanged.AddUniqueDynamic(this, &UHubTopBarWidget::HandleCreditsChanged);
		Inventory->OnCargoChanged.AddUniqueDynamic(this, &UHubTopBarWidget::HandleCargoChanged);
	}
	SetCredits(Inventory ? Inventory->GetCredits() : 0);
	SetCargo(Inventory ? Inventory->GetCargoUsed() : 0, Inventory ? Inventory->GetCargoCapacity() : 0);
}

void UHubTopBarWidget::NativeDestruct()
{
	if (UInventoryComponent* const Inventory = BoundInventory.Get())
	{
		Inventory->OnCreditsChanged.RemoveDynamic(this, &UHubTopBarWidget::HandleCreditsChanged);
		Inventory->OnCargoChanged.RemoveDynamic(this, &UHubTopBarWidget::HandleCargoChanged);
	}
	BoundInventory.Reset();
	UndockButton->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UHubTopBarWidget::HandleCreditsChanged(const int32 NewTotal, const int32 Delta)
{
	SetCredits(NewTotal);
}

void UHubTopBarWidget::HandleCargoChanged(const int32 Used, const int32 Capacity)
{
	SetCargo(Used, Capacity);
}

void UHubTopBarWidget::HandleUndockClicked()
{
	if (AStartechShipPawn* const Ship = GetShip())
	{
		Ship->UndockFromStation();
	}
}

void UHubTopBarWidget::SetCredits(const int32 Credits)
{
	CreditsText->SetText(FText::Format(LOCTEXT("Credits", "{0} credits"), FText::AsNumber(Credits)));
}

void UHubTopBarWidget::SetCargo(const int32 Used, const int32 Capacity)
{
	CargoText->SetText(FText::Format(LOCTEXT("Cargo", "Cargo {0} / {1}"), FText::AsNumber(Used), FText::AsNumber(Capacity)));
}

#undef LOCTEXT_NAMESPACE
