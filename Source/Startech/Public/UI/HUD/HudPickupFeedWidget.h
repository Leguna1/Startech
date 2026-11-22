#pragma once

#include "CoreMinimal.h"
#include "LootBundle.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudPickupFeedWidget.generated.h"

class AStartechShipPawn;
class UInventoryComponent;

/**
 * Bottom right, above the cargo: stacked right-aligned lines for what the player just gained ("+3 iron ore",
 * "+120 credits"), newest at the bottom. Each line fades out at the end of its lifetime; at most the style's line
 * count show. Fed by the inventory's OnItemAdded and credit gains, and by ammo in OnLootCollected.
 */
UCLASS()
class STARTECH_API UHudPickupFeedWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots with the cargo panel. */
	UHudPickupFeedWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the new pawn's inventory and loot event. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Unbinds everything. */
	virtual void NativeDestruct() override;

	/** Drops expired lines and repaints while any show. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** One line of the feed. */
	struct FFeedLine
	{
		/** What it says. */
		FString Text;
		/** Its colour. */
		FLinearColor Color = FLinearColor::White;
		/** HUD time it was added. */
		double Added = 0.0;
	};

	UFUNCTION()
	void HandleItemAdded(FText DisplayName, int32 Quantity);

	UFUNCTION()
	void HandleCreditsChanged(int32 NewTotal, int32 Delta);

	UFUNCTION()
	void HandleLootCollected(const FLootBundle& Taken);

	/** Appends a line, dropping the oldest beyond the style's limit. */
	void AddLine(const FString& Text, const FLinearColor& Color);

	/** Unbinds from the bound inventory and ship. */
	void UnbindAll();

	TWeakObjectPtr<UInventoryComponent> BoundInventory;
	TWeakObjectPtr<AStartechShipPawn> BoundShip;

	/** Lines on screen, oldest first. */
	TArray<FFeedLine> Lines;
};
