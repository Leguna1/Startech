#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MountRowWidget.generated.h"

class AActor;
class UHubButtonWidget;
class UWeaponMountComponent;

/**
 * One weapon mount on the Loadout tab, built in C++: the mount's name, its fitted weapon, and a Primary
 * UHubButtonWidget that fits the next spare ("Fit Shield drainer"; disabled as "No spare"). Fitting goes through
 * FShipFitting. UShopScreenWidget::MountRowWidgetClass points at this class.
 */
UCLASS()
class STARTECH_API UMountRowWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Draws, and takes the mouse for its button. */
	UMountRowWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows InMount. */
	void Setup(UWeaponMountComponent* InMount);

	/** Re-reads the fitted weapon and whether there is a spare to swap in. */
	void Refresh();

protected:
	/** A fixed-height overlay with the swap button on the right. */
	virtual void BuildContent() override;

	/** Binds the button. */
	virtual void NativeConstruct() override;

	/** Unbinds the button. */
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** Fits the next spare. */
	void HandleSwapClicked();

	/** The player's ship. */
	AActor* GetShip() const;

	/** The mount. */
	TWeakObjectPtr<UWeaponMountComponent> Mount;

	/** The mount's name. */
	FString MountName;

	/** The fitted weapon's name, or "Empty". */
	FString WeaponName;

	/** Fits the next spare. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> FitAction;
};
