#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "UpgradeRowWidget.generated.h"

class UHubButtonWidget;

/**
 * One upgrade track in the hub's Upgrades tab, painted in C++ apart from its button:
 *   name and short description | 5 level pips (filled in the Player colour) and the next level's change
 *   ("Hull 300 → 400") | the cost: credits, then minerals as "25 Cobaltine (12)" with the amount held in brackets, each
 *   part in the Credits or Advantage colour when there is enough and DangerText when not | the Upgrade button.
 * A disabled button's label is UShipUpgradeSubsystem::CanUpgrade's reason ("Need 12 more Ferrite"). At the max level
 * the row shows the stat's value and "Max level" instead of the cost and the button.
 * The tab calls Refresh on every upgrade, credits and cargo change.
 */
UCLASS()
class STARTECH_API UUpgradeRowWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Draws, and takes the mouse for the button. */
	UUpgradeRowWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows TrackId. */
	void Setup(FName InTrackId);

	/** Re-reads the level, the next level's change, the cost against the wallet and cargo, and CanUpgrade. */
	void Refresh();

	/** The track shown. */
	FName GetTrackId() const { return TrackId; }

protected:
	/** A fixed-height overlay with the Upgrade button on the right. */
	virtual void BuildContent() override;

	/** Binds the button. */
	virtual void NativeConstruct() override;

	/** Unbinds the button. */
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** One part of the cost line. */
	struct FCostPart
	{
		/** "12,000 credits" or "25 Cobaltine (12)". */
		FString Text;
		/** Enough held or not. */
		FLinearColor Color = FLinearColor::White;
	};

	/** The button: buys the next level. */
	void HandleUpgradeClicked();

	/** The track. */
	FName TrackId;

	/** The track's name. */
	FText Name;

	/** The track's description. */
	FText Description;

	/** The current level. */
	int32 Level = 0;

	/** The track's max level (number of pips). */
	int32 MaxLevel = 0;

	/** The next level's change, or the value at the max level. */
	FString ChangeText;

	/** The cost of the next level; empty at the max level. */
	TArray<FCostPart> CostParts;

	/** The Upgrade button. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> Button;
};
