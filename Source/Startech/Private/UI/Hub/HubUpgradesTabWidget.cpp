#include "UI/Hub/HubUpgradesTabWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InventoryComponent.h"
#include "StartechShipPawn.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubPanelWidget.h"
#include "UI/Hub/UpgradeRowWidget.h"
#include "Upgrades/ShipUpgradeSubsystem.h"

#define LOCTEXT_NAMESPACE "HubUpgradesTab"

namespace HubUpgradesTab
{
	constexpr float Padding = 16.f;
	constexpr float HeaderGap = 4.f;
	constexpr float ListGap = 12.f;
}

void UHubUpgradesTabWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	UOverlaySlot* const PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* const ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	ColumnSlot->SetPadding(FMargin(HubUpgradesTab::Padding));

	UTextBlock* const Title = MakeText(LOCTEXT("Title", "Ship upgrades"), HudStyle.HubTitleFontSize, HudStyle.TextColor, true);
	Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(4.f, 0.f, 0.f, HubUpgradesTab::HeaderGap));

	UTextBlock* const Subtitle = MakeText(
		LOCTEXT("Subtitle", "Permanent: kept on death. Levels 1-2 need Ferrite, 3-4 Cobaltine, 5 Aurium."),
		HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	Column->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(4.f, 0.f, 0.f, HubUpgradesTab::ListGap));

	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* const ScrollSlot = Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	RowBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(RowBox);
}

void UHubUpgradesTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UShipUpgradeSubsystem* const Upgrades = IsDesignTime() ? nullptr : UShipUpgradeSubsystem::Get(this);
	const UUpgradeCatalog* const Catalog = Upgrades ? Upgrades->GetCatalog() : nullptr;
	if (Catalog && Rows.IsEmpty())
	{
		for (const FUpgradeTrack& Track : Catalog->Tracks)
		{
			UUpgradeRowWidget* const Row = WidgetTree->ConstructWidget<UUpgradeRowWidget>(UUpgradeRowWidget::StaticClass());
			RowBox->AddChildToVerticalBox(Row);
			Row->Setup(Track.TrackId);
			Rows.Add(Row);
		}
	}

	if (Upgrades)
	{
		Upgrades->OnUpgradeChanged.AddUniqueDynamic(this, &UHubUpgradesTabWidget::HandleUpgradeChanged);
		BoundUpgrades = Upgrades;
	}

	const AStartechShipPawn* const Ship = GetShip();
	if (UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr)
	{
		Inventory->OnCreditsChanged.AddUniqueDynamic(this, &UHubUpgradesTabWidget::HandleCreditsChanged);
		Inventory->OnItemsChanged.AddUniqueDynamic(this, &UHubUpgradesTabWidget::HandleItemsChanged);
		BoundInventory = Inventory;
	}

	RefreshRows();
}

void UHubUpgradesTabWidget::NativeDestruct()
{
	if (UShipUpgradeSubsystem* const Upgrades = BoundUpgrades.Get())
	{
		Upgrades->OnUpgradeChanged.RemoveDynamic(this, &UHubUpgradesTabWidget::HandleUpgradeChanged);
	}
	BoundUpgrades.Reset();

	if (UInventoryComponent* const Inventory = BoundInventory.Get())
	{
		Inventory->OnCreditsChanged.RemoveDynamic(this, &UHubUpgradesTabWidget::HandleCreditsChanged);
		Inventory->OnItemsChanged.RemoveDynamic(this, &UHubUpgradesTabWidget::HandleItemsChanged);
	}
	BoundInventory.Reset();

	Super::NativeDestruct();
}

void UHubUpgradesTabWidget::HandleUpgradeChanged(const FName TrackId, const int32 NewLevel)
{
	// Other rows can change too: a stat shared by two tracks, or the wallet after a purchase.
	RefreshRows();
}

void UHubUpgradesTabWidget::HandleCreditsChanged(const int32 NewTotal, const int32 Delta)
{
	RefreshRows();
}

void UHubUpgradesTabWidget::HandleItemsChanged()
{
	RefreshRows();
}

void UHubUpgradesTabWidget::RefreshRows()
{
	for (UUpgradeRowWidget* const Row : Rows)
	{
		Row->Refresh();
	}
}

#undef LOCTEXT_NAMESPACE
