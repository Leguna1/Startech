#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubUpgradesTabWidget.generated.h"

class UInventoryComponent;
class UShipUpgradeSubsystem;
class UUpgradeRowWidget;
class UVerticalBox;

/**
 * The hub's Upgrades tab: one cut-corner panel with a header and one UUpgradeRowWidget per track of DA_UpgradeCatalog,
 * in the catalog's order, in a scroll box. Event-driven like the Quests tab: every row refreshes on
 * UShipUpgradeSubsystem::OnUpgradeChanged and the ship's OnCreditsChanged and OnItemsChanged (the costs' colours and
 * the button reasons depend on the wallet and the cargo). Nothing refreshes every frame.
 * Created by UStationHubWidget into its content switcher when WBP_StationHub doesn't place one.
 */
UCLASS()
class STARTECH_API UHubUpgradesTabWidget : public UHubWidgetBase
{
	GENERATED_BODY()

protected:
	/** The panel, the header and an empty row list. */
	virtual void BuildContent() override;

	/** Adds the rows (once) and binds to the upgrade and inventory events. */
	virtual void NativeConstruct() override;

	/** Unbinds. */
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleUpgradeChanged(FName TrackId, int32 NewLevel);

	UFUNCTION()
	void HandleCreditsChanged(int32 NewTotal, int32 Delta);

	UFUNCTION()
	void HandleItemsChanged();

	/** Refreshes every row. */
	void RefreshRows();

	/** Holds the rows. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RowBox;

	/** One per track. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUpgradeRowWidget>> Rows;

	/** The subsystem bound to. */
	TWeakObjectPtr<UShipUpgradeSubsystem> BoundUpgrades;

	/** The ship's inventory bound to. */
	TWeakObjectPtr<UInventoryComponent> BoundInventory;
};
