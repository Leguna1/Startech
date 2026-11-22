#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HudPanelBrush.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubPanelWidget.generated.h"

/**
 * A background that fills its slot: a cut-corner HUD panel (M_HUD_ChamferPanel), or the hub's full-screen backdrop.
 * Paint only. Put it first in an Overlay so the content draws over it.
 */
UCLASS()
class STARTECH_API UHubPanelWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Never takes the mouse. */
	UHubPanelWidget(const FObjectInitializer& ObjectInitializer);

	/** True draws the style's HubBackdropColor as a plain box instead of a panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	bool bBackdrop = false;

	/** Outline colour of the panel, as a fraction of the Player colour's opacity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "0", ClampMax = "1"))
	float BorderOpacity = 0.35f;

protected:
	/** Makes the panel material instance. */
	virtual void NativeConstruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** The panel background. */
	UPROPERTY(Transient)
	FHudPanelBrush Panel;
};
