#pragma once

#include "CoreMinimal.h"
#include "ShopQuantity.h"
#include "UI/Hub/HubWidgetBase.h"
#include "ShopQuantityBarWidget.generated.h"

class UHorizontalBox;
class UHubButtonWidget;
class UTextBlock;

/** Broadcast when the player picks a different quantity chip. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShopQuantityChanged, EShopQuantity);

/**
 * The top of the shop tab, built in C++: a row of five Toggle UHubButtonWidget chips (×1, ×10, ×100, ×1,000, Max), one
 * of them selected, and under it the status line that reports the last trade ("Bought 100 Ferrite for 1,200 cr") or why
 * it failed. UShopScreenWidget inserts it under its tabs; the chips are hidden on tabs that trade one at a time
 * (weapons) and on the Loadout tab.
 */
UCLASS()
class STARTECH_API UShopQuantityBarWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Selects Quantity's chip, without broadcasting. */
	void SetQuantity(EShopQuantity Quantity);

	/** The selected quantity. */
	EShopQuantity GetQuantity() const { return Selected; }

	/** Shows or hides the chips; the status line stays. */
	void SetChipsVisible(bool bVisible);

	/** Sets the status line. Empty clears it. */
	void SetStatus(const FText& Text, const FLinearColor& Color);

	/** Fires when the player clicks another chip. */
	FOnShopQuantityChanged OnQuantityChanged;

protected:
	/** The chips and the status line. */
	virtual void BuildContent() override;

	/** Binds the chips. */
	virtual void NativeConstruct() override;

	/** Unbinds the chips. */
	virtual void NativeDestruct() override;

private:
	/** A chip was clicked. */
	void HandleChipClicked(EShopQuantity Quantity);

	/** The selected quantity. */
	EShopQuantity Selected = EShopQuantity::One;

	/** The chips' row. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ChipRow;

	/** One per quantity, in FShopQuantity::All order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHubButtonWidget>> Chips;

	/** The last trade's result. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};
