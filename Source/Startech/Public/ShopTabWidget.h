#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "ShopTabWidget.generated.h"

class UHubButtonWidget;
class UShopScreenWidget;

/**
 * One tab on the shop screen (a shop, or the Loadout), built in C++: a Toggle UHubButtonWidget that is selected while
 * its tab is shown. UShopScreenWidget::TabWidgetClass points at this class.
 */
UCLASS()
class STARTECH_API UShopTabWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** TabIndex is passed back to the screen's SelectShop when the tab is clicked. */
	void Setup(UShopScreenWidget* InScreen, int32 InTabIndex, const FText& Label);

	/** Shows the tab as the selected one, or not. */
	void SetSelected(bool bInSelected);

protected:
	/** The Toggle button, with a little space after it. */
	virtual void BuildContent() override;

	/** Binds the button. */
	virtual void NativeConstruct() override;

	/** Unbinds the button. */
	virtual void NativeDestruct() override;

private:
	/** Selects this tab on the screen, unless it already is. */
	void HandleClicked();

	/** The screen that owns this tab. */
	TWeakObjectPtr<UShopScreenWidget> Screen;

	/** This tab's index on the screen. */
	int32 TabIndex = INDEX_NONE;

	/** The button. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> Button;
};
