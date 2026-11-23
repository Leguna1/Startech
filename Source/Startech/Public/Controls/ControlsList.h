#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UControlsHelpData;
class UInputAction;
class UInputMappingContext;
struct FControlsHelpRow;

/** One keycap of a Controls row. */
struct STARTECH_API FControlsKeyCap
{
	/** What the cap shows ("W", "Mouse", "Fly in"). */
	FText Text;

	/** Drawn dimmed: no real key ("Auto", "Fly in", "Unbound"). */
	bool bDimmed = false;
};

/** One row as the Controls screen draws it. */
struct STARTECH_API FControlsListRow
{
	FText DisplayName;
	FText Description;
	TArray<FControlsKeyCap> Keys;

	/** True joins the caps with "+", false with "/". */
	bool bCombination = false;

	/** True for a mapped action no row of DA_ControlsHelp describes (listed under "Other"). */
	bool bUndescribed = false;

	/** The Input Actions behind the row (empty for fixed and keyless rows). */
	TArray<const UInputAction*> Actions;
};

/** One heading and its rows. */
struct STARTECH_API FControlsListGroup
{
	FText DisplayName;
	TArray<FControlsListRow> Rows;
};

/**
 * Builds what the Controls screen shows from DA_ControlsHelp and the keys its mapping contexts hold now, so a key
 * changed in IMC_Ship shows at once. No widget code: the screen and the tests share it.
 */
struct STARTECH_API FControlsList
{
	/**
	 * The groups in the data's order, each with its rows; empty groups are left out. Every action mapped in the data's
	 * contexts appears: one no row names goes under OtherGroupName, as its asset name (without "IA_") and its keys, with
	 * a warning logged. OutUndescribed, if given, receives those actions.
	 */
	static TArray<FControlsListGroup> Build(const UControlsHelpData& Data, TArray<const UInputAction*>* OutUndescribed = nullptr);

	/** Every action Contexts map, in mapping order, once each. */
	static TArray<const UInputAction*> GetMappedActions(const TArray<const UInputMappingContext*>& Contexts);

	/** Every key Contexts map to Action, in mapping order, once each. */
	static TArray<FKey> GetMappedKeys(const TArray<const UInputMappingContext*>& Contexts, const UInputAction& Action);

	/**
	 * Text with each {Name} placeholder replaced by the current key labels of the action whose asset name is IA_Name
	 * ("{Interact}" → "F", "{Throttle}" → "W / S"; "Unbound" when it has no key). The action must be mapped in the data's
	 * contexts or named by a row. An unknown name stays as written, with a warning logged. Used for row descriptions
	 * and the tips, so they never name a stale key.
	 */
	static FText FormatWithKeys(const UControlsHelpData& Data, const FText& Text);

	/** An action's name for the screen: its asset name without "IA_" ("IA_Throttle" → "Throttle"). */
	static FText GetActionFallbackName(const UInputAction& Action);

	/** The row's keycaps, joined by " + " or " / ", for logs and tests ("W / S", "[Fly in]"). */
	static FString DescribeKeys(const FControlsListRow& Row);
};
