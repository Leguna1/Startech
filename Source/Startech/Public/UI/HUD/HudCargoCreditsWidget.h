#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HudElementWidget.h"
#include "UI/HUD/HudPanelBrush.h"
#include "HudCargoCreditsWidget.generated.h"

class UInventoryComponent;

/**
 * Bottom right: "Cargo" with "Used / Capacity" and a segmented bar (Advantage colour, Hull colour from the style's
 * warning fraction, Danger when full), and the credits in the Credits colour with thousands separators and "cr". A
 * credits change counts to the new total and briefly tints the text, Advantage for a gain and Danger for a loss.
 * Event-driven: wrap it in an Invalidation Box.
 */
UCLASS()
class STARTECH_API UHudCargoCreditsWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots fourth, after the action bar. */
	UHudCargoCreditsWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the new pawn's inventory. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads cargo and credits; credits show the total at once, without counting up. */
	virtual void RefreshFromPawn() override;

	/** Unbinds from the inventory. */
	virtual void NativeDestruct() override;

	/** Repaints while the credits count or tint animates. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Rebuilds the panel material instance. */
	virtual void HandleStyleChanged() override;

private:
	UFUNCTION()
	void HandleCargoChanged(int32 Used, int32 Capacity);

	UFUNCTION()
	void HandleCreditsChanged(int32 NewTotal, int32 Delta);

	/** The credits value shown now, part way through the count. */
	int64 GetDisplayedCredits() const;

	/** True while the count or the tint animates. */
	bool IsAnimating() const;

	/** Unbinds from the bound inventory. */
	void UnbindInventory();

	/** The inventory this widget follows. */
	TWeakObjectPtr<UInventoryComponent> BoundInventory;

	/** Panel background. */
	UPROPERTY(Transient)
	FHudPanelBrush Panel;

	int32 CargoUsed = 0;
	int32 CargoCapacity = 0;

	/** Where the count started from. */
	int64 CountFrom = 0;

	/** The credits total the count heads for. */
	int64 CountTo = 0;

	/** HUD time the count started; negative when not counting. */
	double CountStart = -1.0;

	/** Sign of the last change: 1 gain, -1 loss. */
	int32 LastDeltaSign = 0;
};
