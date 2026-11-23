#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "ControlsHelpData.generated.h"

class UInputAction;
class UInputMappingContext;

STARTECH_API DECLARE_LOG_CATEGORY_EXTERN(LogStartechControls, Log, All);

/** How a row's keys are drawn between their keycaps. */
UENUM(BlueprintType)
enum class EControlsKeyJoin : uint8
{
	/** Any of the keys: "W / S". */
	Alternatives,
	/** All of the keys together: "Shift + click". */
	Combination,
};

/** A short label drawn on a keycap instead of the engine's display name ("Mouse" for "Mouse XY 2D-Axis"). */
USTRUCT(BlueprintType)
struct STARTECH_API FControlsKeyLabel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText Label;
};

/** One heading of the Controls screen, in the order of UControlsHelpData::Groups. */
USTRUCT(BlueprintType)
struct STARTECH_API FControlsHelpGroup
{
	GENERATED_BODY()

	/** What rows name to join this group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FName GroupId;

	/** The heading ("Flight"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText DisplayName;
};

/**
 * One line of the Controls screen. Its keys come, in this order of preference, from:
 * - Actions: every key the mapping contexts map to these actions, in mapping order (merged: "1 / 2 / 3 / 4");
 * - FixedKeys: inputs handled outside Enhanced Input (the shop's 1-5, Shift-click);
 * - KeyNote: no key at all, drawn as one dimmed keycap ("Auto", "Fly in").
 */
USTRUCT(BlueprintType)
struct STARTECH_API FControlsHelpRow
{
	GENERATED_BODY()

	/** FControlsHelpGroup::GroupId of the heading this row goes under. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FName GroupId;

	/** The action's name ("Throttle"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText DisplayName;

	/** One line on what it does. {Name} is replaced by the current keys of IA_Name ("{Interact} closes it" → "F closes it"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls", meta = (MultiLine = "true"))
	FText Description;

	/** The Input Actions this row describes; their keys are read from the mapping contexts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<TObjectPtr<UInputAction>> Actions;

	/** Keys handled outside Enhanced Input (widget key handling), used when Actions is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FKey> FixedKeys;

	/** Text of one dimmed keycap for an input with no key ("Auto", "Fly in"), used when there are no keys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText KeyNote;

	/** Alternatives ("W / S") or a combination ("Shift + click"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	EControlsKeyJoin Join = EControlsKeyJoin::Alternatives;

	/** Labels for this row only, over UControlsHelpData::KeyLabels ("Shift" and "click" in Shift-click). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FControlsKeyLabel> RowKeyLabels;
};

/**
 * DA_ControlsHelp: everything the Controls screen (UControlsScreenWidget) shows except the keys, which come from
 * MappingContexts at runtime. Made by Tools/Menu/setup_controls_help.py; chosen in UStartechSettings::ControlsHelp.
 * A mapped action that no row names still shows, under OtherGroupName, with a warning in the log.
 */
UCLASS(BlueprintType)
class STARTECH_API UControlsHelpData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** UStartechSettings::ControlsHelp, loaded; null (with an error logged once) when it is not set. */
	static const UControlsHelpData* Load();

	/** The keycap label for Key: Row's own labels, then KeyLabels, then the engine's display name. */
	FText GetKeyLabel(const FKey& Key, const FControlsHelpRow* Row) const;

	/** The mapping contexts whose keys the screen shows (IMC_Ship). Read as assets, so it works in the main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<TObjectPtr<UInputMappingContext>> MappingContexts;

	/** The headings, in order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FControlsHelpGroup> Groups;

	/** The rows, in order within their group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FControlsHelpRow> Rows;

	/** Short keycap labels; any other key uses the engine's display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TArray<FControlsKeyLabel> KeyLabels;

	/** The heading for mapped actions no row describes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText OtherGroupName;

	/** The dimmed keycap of a row whose actions have no key in MappingContexts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText UnboundKeyText;

	/** The heading of the tips box ("How combat works"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FText TipsTitle;

	/** The tips box's lines. {Name} placeholders work as in row descriptions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls", meta = (MultiLine = "true"))
	TArray<FText> Tips;
};
