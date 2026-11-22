#include "ShopScreenWidget.h"

#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "InventoryComponent.h"
#include "MountRowWidget.h"
#include "ShopData.h"
#include "ShopEntryWidget.h"
#include "ShopQuantityBarWidget.h"
#include "UI/HUD/StartechHudStyle.h"
#include "ShopTabWidget.h"
#include "ShopTrading.h"
#include "TradeableData.h"
#include "StartechShipPawn.h"
#include "Station.h"
#include "WeaponMountComponent.h"
#include "WeaponSystemComponent.h"

#define LOCTEXT_NAMESPACE "ShopScreenWidget"

void UShopScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildQuantityBar();
}

void UShopScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ColumnHeaders)
	{
		ColumnHeaders->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (const AStartechShipPawn* const Ship = GetShip())
	{
		Station = Ship->GetDockedStation();

		if (UInventoryComponent* const Inventory = Ship->GetInventory())
		{
			Inventory->OnCreditsChanged.AddUniqueDynamic(this, &UShopScreenWidget::HandleCreditsChanged);
			Inventory->OnItemsChanged.AddUniqueDynamic(this, &UShopScreenWidget::HandleItemsChanged);
			Inventory->OnWeaponsChanged.AddUniqueDynamic(this, &UShopScreenWidget::HandleWeaponsChanged);
			BoundInventory = Inventory;
		}

		if (UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem())
		{
			Weapons->OnAmmoChanged.AddUniqueDynamic(this, &UShopScreenWidget::HandleAmmoChanged);
			BoundWeapons = Weapons;
		}
	}

	BuildTabs();
	SelectShop(0);
}

void UShopScreenWidget::BuildQuantityBar()
{
	if (QuantityBar)
	{
		return;
	}

	UPanelWidget* const Parent = TabContainer ? TabContainer->GetParent() : nullptr;
	if (!Parent)
	{
		return;
	}

	// Inserted before the frame's Slate widgets exist, so the bar lands right under the tabs; once built, a live
	// panel would append it at the end instead.
	QuantityBar = CreateWidget<UShopQuantityBarWidget>(this, UShopQuantityBarWidget::StaticClass());
	UPanelSlot* const BarSlot = Parent->InsertChildAt(Parent->GetChildIndex(TabContainer) + 1, QuantityBar);

	// Lined up with the tabs: the same left and right inset.
	const UVerticalBoxSlot* const TabSlot = Cast<UVerticalBoxSlot>(TabContainer->Slot);
	if (UVerticalBoxSlot* const VerticalSlot = Cast<UVerticalBoxSlot>(BarSlot); VerticalSlot && TabSlot)
	{
		const FMargin TabPadding = TabSlot->GetPadding();
		VerticalSlot->SetPadding(FMargin(TabPadding.Left, 0.f, TabPadding.Right, 0.f));
	}
	QuantityBar->SetQuantity(Quantity);
	QuantityBar->OnQuantityChanged.AddUObject(this, &UShopScreenWidget::HandleQuantityChanged);
}

void UShopScreenWidget::SetQuantity(const EShopQuantity NewQuantity)
{
	Quantity = NewQuantity;
	if (QuantityBar)
	{
		QuantityBar->SetQuantity(Quantity);
	}
	for (UShopEntryWidget* const Row : Rows)
	{
		Row->SetQuantity(Quantity);
	}
}

void UShopScreenWidget::HandleQuantityChanged(const EShopQuantity NewQuantity)
{
	SetQuantity(NewQuantity);
}

bool UShopScreenWidget::HandleShortcutKey(const FKey& Key)
{
	EShopQuantity Picked;
	if (!UsesQuantities() || !FShopQuantity::FromKey(Key, Picked))
	{
		return false;
	}
	SetQuantity(Picked);
	return true;
}

bool UShopScreenWidget::UsesQuantities() const
{
	const AStation* const DockedAt = Station.Get();
	if (!DockedAt || SelectedTab == INDEX_NONE || IsLoadoutSelected())
	{
		return false;
	}
	const UShopData* const Shop = DockedAt->GetShops()[SelectedTab].Get();
	return Shop && Shop->Entries.ContainsByPredicate([](const FShopEntry& Entry)
	{
		return Entry.Good && !FShopTrading::IsSingleUnitGood(Entry);
	});
}

void UShopScreenWidget::HandleRowTraded(const FTradeOutcome& Outcome)
{
	if (!QuantityBar || !Outcome.Good)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = QuantityBar->GetHudStyle();
	if (!Outcome.IsSuccess())
	{
		StatusText = Outcome.Reason;
		QuantityBar->SetStatus(StatusText, HudStyle.DangerTextColor);
		return;
	}

	StatusText = FText::Format(Outcome.Direction == ETradeDirection::Buy
			? LOCTEXT("Bought", "Bought {0} {1} for {2} cr")
			: LOCTEXT("Sold", "Sold {0} {1} for {2} cr"),
		FText::AsNumber(Outcome.Quantity), Outcome.Good->DisplayName, FText::AsNumber(Outcome.Credits));
	QuantityBar->SetStatus(StatusText, HudStyle.AdvantageColor);
}

void UShopScreenWidget::NativeDestruct()
{
	if (UInventoryComponent* const Inventory = BoundInventory.Get())
	{
		Inventory->OnCreditsChanged.RemoveDynamic(this, &UShopScreenWidget::HandleCreditsChanged);
		Inventory->OnItemsChanged.RemoveDynamic(this, &UShopScreenWidget::HandleItemsChanged);
		Inventory->OnWeaponsChanged.RemoveDynamic(this, &UShopScreenWidget::HandleWeaponsChanged);
	}

	if (UWeaponSystemComponent* const Weapons = BoundWeapons.Get())
	{
		Weapons->OnAmmoChanged.RemoveDynamic(this, &UShopScreenWidget::HandleAmmoChanged);
	}

	Super::NativeDestruct();
}

void UShopScreenWidget::SelectShop(const int32 TabIndex)
{
	// The Loadout tab (index == shop count) is valid whenever docked, even at a station with no shops.
	SelectedTab = Station.IsValid() && TabIndex >= 0 && TabIndex <= GetShopCount() ? TabIndex : INDEX_NONE;

	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		Tabs[Index]->SetSelected(Index == SelectedTab);
	}

	BuildEntries();
	if (QuantityBar)
	{
		QuantityBar->SetChipsVisible(UsesQuantities());
		QuantityBar->SetStatus(FText::GetEmpty(), FLinearColor::White);
	}
	StatusText = FText::GetEmpty();
	Refresh();
}

int32 UShopScreenWidget::GetShopCount() const
{
	const AStation* const DockedAt = Station.Get();
	return DockedAt ? DockedAt->GetShops().Num() : 0;
}

bool UShopScreenWidget::IsLoadoutSelected() const
{
	return SelectedTab != INDEX_NONE && SelectedTab == GetShopCount();
}

void UShopScreenWidget::BuildTabs()
{
	TabContainer->ClearChildren();
	Tabs.Reset();

	const AStation* const DockedAt = Station.Get();
	if (!DockedAt || !TabWidgetClass)
	{
		return;
	}

	const TArray<TObjectPtr<UShopData>>& Shops = DockedAt->GetShops();
	for (int32 Index = 0; Index <= Shops.Num(); ++Index)
	{
		// The tab after the last shop is the Loadout tab.
		const bool bLoadout = Index == Shops.Num();
		const FText Label = bLoadout ? LOCTEXT("Loadout", "Loadout")
			: Shops[Index] ? Shops[Index]->DisplayName : FText::GetEmpty();

		UShopTabWidget* const Tab = CreateWidget<UShopTabWidget>(this, TabWidgetClass);
		Tab->Setup(this, Index, Label);
		TabContainer->AddChild(Tab);
		Tabs.Add(Tab);
	}
}

void UShopScreenWidget::BuildEntries()
{
	EntryContainer->ClearChildren();
	Rows.Reset();
	MountRows.Reset();

	if (IsLoadoutSelected())
	{
		BuildMountRows();
	}
	else
	{
		BuildShopRows();
	}
}

void UShopScreenWidget::BuildShopRows()
{
	const AStation* const DockedAt = Station.Get();
	UShopData* const Shop = DockedAt && SelectedTab != INDEX_NONE ? DockedAt->GetShops()[SelectedTab].Get() : nullptr;

	if (ShopNameText)
	{
		ShopNameText->SetText(Shop ? Shop->DisplayName : FText::GetEmpty());
	}

	if (!Shop || !EntryWidgetClass)
	{
		return;
	}

	for (int32 Index = 0; Index < Shop->Entries.Num(); ++Index)
	{
		// Rows with nothing to offer are left out.
		const FShopEntry& Entry = Shop->Entries[Index];
		if (!FShopTrading::IsBuyOffered(Entry) && !FShopTrading::IsSellOffered(Entry))
		{
			continue;
		}

		UShopEntryWidget* const Row = CreateWidget<UShopEntryWidget>(this, EntryWidgetClass);
		EntryContainer->AddChild(Row);
		Row->SetQuantity(Quantity);
		Row->Setup(Shop, Index);
		Row->OnTraded.AddUObject(this, &UShopScreenWidget::HandleRowTraded);
		Rows.Add(Row);
	}
}

void UShopScreenWidget::BuildMountRows()
{
	if (ShopNameText)
	{
		ShopNameText->SetText(LOCTEXT("Loadout", "Loadout"));
	}

	const AStartechShipPawn* const Ship = GetShip();
	if (!Ship || !MountRowWidgetClass)
	{
		return;
	}

	TInlineComponentArray<UWeaponMountComponent*> Mounts;
	Ship->GetComponents(Mounts);

	for (UWeaponMountComponent* const Mount : Mounts)
	{
		UMountRowWidget* const Row = CreateWidget<UMountRowWidget>(this, MountRowWidgetClass);
		EntryContainer->AddChild(Row);
		Row->Setup(Mount);
		MountRows.Add(Row);
	}
}

void UShopScreenWidget::Refresh()
{
	if (const UInventoryComponent* const Inventory = BoundInventory.Get())
	{
		const int32 Used = Inventory->GetCargoUsed();
		const int32 Capacity = Inventory->GetCargoCapacity();

		if (CreditsText)
		{
			CreditsText->SetText(FText::Format(LOCTEXT("Credits", "Credits: {0}"), FText::AsNumber(Inventory->GetCredits())));
		}
		if (CargoText)
		{
			CargoText->SetText(FText::Format(LOCTEXT("Cargo", "Cargo: {0} / {1}"), FText::AsNumber(Used), FText::AsNumber(Capacity)));
		}

		if (CargoBar)
		{
			CargoBar->SetPercent(Capacity > 0 ? FMath::Clamp(static_cast<float>(Used) / Capacity, 0.f, 1.f) : 0.f);
		}
	}

	for (UShopEntryWidget* const Row : Rows)
	{
		Row->Refresh();
	}

	for (UMountRowWidget* const Row : MountRows)
	{
		Row->Refresh();
	}
}

void UShopScreenWidget::HandleCreditsChanged(const int32 NewCredits, const int32 Delta)
{
	Refresh();
}

void UShopScreenWidget::HandleItemsChanged()
{
	Refresh();
}

void UShopScreenWidget::HandleWeaponsChanged()
{
	Refresh();
}

void UShopScreenWidget::HandleAmmoChanged(UAmmoData* const Ammo, const float NewCount)
{
	Refresh();
}

AStartechShipPawn* UShopScreenWidget::GetShip() const
{
	return Cast<AStartechShipPawn>(GetOwningPlayerPawn());
}

#undef LOCTEXT_NAMESPACE
