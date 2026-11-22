#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubTopBarWidget.generated.h"

class UHubButtonWidget;
class UInventoryComponent;
class UTextBlock;

/**
 * The hub's full-width top bar: the docked station's name on the left (14 pt), then credits in the Credits colour and
 * "Cargo used / capacity", both following the inventory's events, and an Undock button on the right that moves the
 * ship out of the docking zone and closes the hub.
 */
UCLASS()
class STARTECH_API UHubTopBarWidget : public UHubWidgetBase
{
	GENERATED_BODY()

protected:
	/** A panel with the station name, credits, cargo and the Undock button in a row. */
	virtual void BuildContent() override;

	/** Reads the station and binds to the ship's inventory. */
	virtual void NativeConstruct() override;

	/** Unbinds from the inventory. */
	virtual void NativeDestruct() override;

private:
	/** The inventory's credits event. */
	UFUNCTION()
	void HandleCreditsChanged(int32 NewTotal, int32 Delta);

	/** The inventory's cargo event. */
	UFUNCTION()
	void HandleCargoChanged(int32 Used, int32 Capacity);

	/** Undocks the ship. */
	void HandleUndockClicked();

	/** Shows the credits total. */
	void SetCredits(int32 Credits);

	/** Shows the cargo use. */
	void SetCargo(int32 Used, int32 Capacity);

	/** The station's name. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StationText;

	/** "12,400 credits". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CreditsText;

	/** "Cargo 40 / 120". */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CargoText;

	/** Undocks and closes the hub. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> UndockButton;

	/** The inventory bound to. */
	TWeakObjectPtr<UInventoryComponent> BoundInventory;
};
