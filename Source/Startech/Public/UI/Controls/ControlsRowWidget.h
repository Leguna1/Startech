#pragma once

#include "CoreMinimal.h"
#include "Controls/ControlsList.h"
#include "UI/Hub/HubWidgetBase.h"
#include "ControlsRowWidget.generated.h"

class UControlsKeyCapWidget;
class UTextBlock;
class UWrapBox;

/**
 * One action on the Controls screen: its keycaps on the left (joined by "/" or "+"), the action's name, and a one-line
 * description. One widget per action, so key rebinding can later be added here.
 */
UCLASS()
class STARTECH_API UControlsRowWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Shows Row. */
	void Setup(const FControlsListRow& Row);

	/** The keycaps as drawn ("W / S", "[Fly in]"), for logs and tests. */
	FString DescribeKeys() const;

	/** The name as drawn. */
	FText GetDisplayName() const;

	/** The description as drawn. */
	FText GetDescription() const;

protected:
	/** The key column, the name and the description, in a row. */
	virtual void BuildContent() override;

private:
	/** Holds the keycaps and the separators between them. */
	UPROPERTY(Transient)
	TObjectPtr<UWrapBox> KeyBox;

	/** The keycaps, in order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UControlsKeyCapWidget>> Caps;

	/** The action's name. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	/** What it does. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;

	/** True when the caps are joined by "+". */
	bool bCombination = false;

	/** Whether each cap in Caps is dimmed (no real key). */
	TArray<bool> CapDimmed;
};
