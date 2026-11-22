#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionBarTypes.generated.h"

class UInputAction;
class UTexture2D;

/** What a slot on the action bar holds. Each kind has one UActionSlotProvider (FActionSlotProviderFactory). */
UENUM(BlueprintType)
enum class EActionSlotKind : uint8
{
	/** An ammo type: pressing selects it for its weapon group and makes the group active. */
	Ammo
};

/** How a slot's icon is drawn. */
UENUM(BlueprintType)
enum class EActionSlotIcon : uint8
{
	/** No icon. */
	None,
	/** Vertical bars, one per tier (laser ammo). */
	Bars,
	/** Downward chevrons, one per tier (shield drain ammo). */
	Chevrons,
	/** The item's icon texture (UTradeableData::Icon). */
	Texture
};

/** What a slot looks like apart from its state: its icon. Filled by the slot kind's provider. */
USTRUCT(BlueprintType)
struct STARTECH_API FActionSlotVisual
{
	GENERATED_BODY()

	/** The icon's shape. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	EActionSlotIcon Icon = EActionSlotIcon::None;

	/** How many bars or chevrons. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	int32 Tier = 1;

	/** The texture for EActionSlotIcon::Texture. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<UTexture2D> Texture;
};

/** One slot of the action bar layout (UActionBarLayout). Plain data, so a player-made layout can use it later. */
USTRUCT(BlueprintType)
struct STARTECH_API FActionBarSlot
{
	GENERATED_BODY()

	/** What the slot holds, which picks its provider. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	EActionSlotKind Kind = EActionSlotKind::Ammo;

	/** The item in the slot, by its ItemTag in UStartechSettings::ItemCatalog (for example Item.Ammo.LL1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (Categories = "Item"))
	FGameplayTag ItemTag;

	/**
	 * The label shown above the slot's group ("Lasers", "Drainer"). Neighbouring slots with the same label form one
	 * group on the bar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	FText GroupLabel;

	/** The input action that presses this slot (IA_Slot1...). Its key in IMC_Ship is the slot's key label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<UInputAction> InputAction;
};

/**
 * Everything the action bar draws for one slot, whatever the slot holds. Filled by the slot kind's provider; the
 * widget reads only this. Fields a kind doesn't use stay at their defaults.
 */
USTRUCT(BlueprintType)
struct STARTECH_API FActionSlotState
{
	GENERATED_BODY()

	/** This slot is its group's current choice (the selected ammo of its weapon group). */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	bool bSelected = false;

	/** This slot's group is the one in use (the active weapon group, which the trigger fires). */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	bool bInActiveGroup = false;

	/** How many are held; -1 for unlimited (drawn as "∞"). */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	int32 Count = 0;

	/** Above zero but below the low threshold. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	bool bLow = false;

	/** None left. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	bool bEmpty = false;

	/** A target is selected and it is beyond this slot's reach (its weapon group's range). */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	bool bOutOfRange = false;

	/** The slot can't be used at all (for later kinds). */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	bool bDisabled = false;

	/** Cooldown left, 1 just used to 0 ready. Unused by ammo. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	float CooldownFraction = 0.f;

	/** Seconds of cooldown left. Unused by ammo. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	float CooldownSecondsLeft = 0.f;

	/** How much of an active effect is left, 1 to 0. Unused by ammo. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	float ActiveFraction = 0.f;
};
