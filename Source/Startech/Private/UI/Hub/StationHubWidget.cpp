#include "UI/Hub/StationHubWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Runtime/QuestSubsystem.h"
#include "ShopScreenWidget.h"
#include "StartechShipPawn.h"
#include "UI/Hub/HubQuestsTabWidget.h"
#include "UI/Hub/HubTabRailWidget.h"
#include "UI/Hub/HubTopBarWidget.h"
#include "UI/Hub/HubUpgradesTabWidget.h"

UStationHubWidget::UStationHubWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UStationHubWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!UpgradesTab)
	{
		UpgradesTab = WidgetTree->ConstructWidget<UHubUpgradesTabWidget>(UHubUpgradesTabWidget::StaticClass(), TEXT("UpgradesTab"));
		ContentSwitcher->AddChild(UpgradesTab);
	}

	TabRail->OnTabSelected.AddUObject(this, &UStationHubWidget::SelectTab);

	// Quests when something is ready to turn in, otherwise where the player left off.
	const UQuestSubsystem* const Quests = GetQuests();
	const bool bAnyReady = Quests && !Quests->GetQuestsByStatus(EQuestStatus::ReadyToTurnIn).IsEmpty();
	const AStartechShipPawn* const Ship = GetShip();
	SelectTab(bAnyReady || !Ship ? EHubTab::Quests : Ship->GetLastHubTab());
}

void UStationHubWidget::NativeDestruct()
{
	TabRail->OnTabSelected.RemoveAll(this);
	Super::NativeDestruct();
}

void UStationHubWidget::SelectTab(const EHubTab Tab)
{
	TabRail->SetSelectedTab(Tab);
	UWidget* Content = QuestsTab;
	if (Tab == EHubTab::Shop)
	{
		Content = ShopScreen;
	}
	else if (Tab == EHubTab::Upgrades)
	{
		Content = UpgradesTab;
	}
	ContentSwitcher->SetActiveWidget(Content);

	if (AStartechShipPawn* const Ship = GetShip())
	{
		Ship->SetLastHubTab(Tab);
	}
}

FReply UStationHubWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	AStartechShipPawn* const Ship = GetShip();
	const FKey Key = InKeyEvent.GetKey();
	if (Ship && !InKeyEvent.IsRepeat() && (Key == EKeys::Escape || Ship->IsInteractKey(Key)))
	{
		Ship->CloseHub();
		return FReply::Handled();
	}

	// Keys 1-5 pick the shop's quantity while the Shop tab is open.
	if (ContentSwitcher->GetActiveWidget() == ShopScreen && ShopScreen->HandleShortcutKey(Key))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
