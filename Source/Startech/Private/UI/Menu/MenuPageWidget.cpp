#include "UI/Menu/MenuPageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Controls/ControlsHelpData.h"

namespace MenuPage
{
	/** Above the menu's own widgets and its dialog. */
	constexpr int32 ZOrder = 100;
}

UMenuPageWidget::UMenuPageWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
}

UMenuPageWidget* UMenuPageWidget::AddToMenu(UUserWidget& Menu, const TSubclassOf<UMenuPageWidget> Class, const FName Name)
{
	UCanvasPanel* const Canvas = Menu.WidgetTree ? Cast<UCanvasPanel>(Menu.WidgetTree->RootWidget) : nullptr;
	if (!Canvas || !Class)
	{
		UE_LOG(LogStartechControls, Error, TEXT("%s has no root canvas panel: its %s page can't be added."),
			*Menu.GetClass()->GetName(), *Name.ToString());
		return nullptr;
	}

	UMenuPageWidget* const Page = Menu.WidgetTree->ConstructWidget<UMenuPageWidget>(Class, Name);
	UCanvasPanelSlot* const PageSlot = Canvas->AddChildToCanvas(Page);
	PageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	PageSlot->SetOffsets(FMargin(0.f));
	PageSlot->SetZOrder(MenuPage::ZOrder);
	return Page;
}

void UMenuPageWidget::Open()
{
	if (bOpen)
	{
		return;
	}
	OnPageOpened();
	bOpen = true;
	SetVisibility(ESlateVisibility::Visible);
}

void UMenuPageWidget::Close()
{
	if (!bOpen)
	{
		return;
	}
	OnPageClosing();
	bOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	OnClosed.Broadcast();
}

FReply UMenuPageWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bOpen && InKeyEvent.GetKey() == EKeys::Escape && !InKeyEvent.IsRepeat())
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
