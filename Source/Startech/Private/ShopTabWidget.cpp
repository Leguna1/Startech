#include "ShopTabWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "ShopScreenWidget.h"
#include "UI/Hub/HubButtonWidget.h"

namespace ShopTab
{
	/** Space after each tab. */
	constexpr float Gap = 6.f;
}

void UShopTabWidget::BuildContent()
{
	// The size box's right padding keeps the tabs apart wherever the screen places them.
	USizeBox* const Root = WidgetTree->ConstructWidget<USizeBox>();
	WidgetTree->RootWidget = Root;

	Button = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	Button->SetVariant(EHubButtonVariant::Toggle);
	if (USizeBoxSlot* const ButtonSlot = Cast<USizeBoxSlot>(Root->AddChild(Button)))
	{
		ButtonSlot->SetPadding(FMargin(0.f, 0.f, ShopTab::Gap, 0.f));
	}
}

void UShopTabWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Button->OnClicked.AddUObject(this, &UShopTabWidget::HandleClicked);
}

void UShopTabWidget::NativeDestruct()
{
	Button->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void UShopTabWidget::Setup(UShopScreenWidget* const InScreen, const int32 InTabIndex, const FText& Label)
{
	Screen = InScreen;
	TabIndex = InTabIndex;
	Button->SetLabel(Label);
	SetSelected(false);
}

void UShopTabWidget::SetSelected(const bool bInSelected)
{
	Button->SetSelected(bInSelected);
}

void UShopTabWidget::HandleClicked()
{
	if (Button->IsSelected())
	{
		return;
	}

	if (UShopScreenWidget* const Owner = Screen.Get())
	{
		Owner->SelectShop(TabIndex);
	}
}
