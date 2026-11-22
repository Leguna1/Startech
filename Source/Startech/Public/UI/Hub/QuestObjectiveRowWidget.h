#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "QuestObjectiveRowWidget.generated.h"

class USizeBox;

/**
 * One objective in the hub's quest details, painted in C++: a checkbox (filled in the Advantage colour when done),
 * the description with {Current} and {Required} filled in, a right-aligned "Current / Required", and a segmented
 * progress bar underneath for objectives that count above 1.
 */
UCLASS()
class STARTECH_API UQuestObjectiveRowWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Display only: never takes the mouse. */
	UQuestObjectiveRowWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows the objective's text and count, and sizes the row (taller with a progress bar). */
	void SetObjective(const FText& InText, int32 InCurrent, int32 InRequired);

protected:
	/** A size box giving the row its height. */
	virtual void BuildContent() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** True when the objective counts above 1 and shows a progress bar. */
	bool HasBar() const { return Required > 1; }

	/** Sets the size box's height: one line, plus the bar when there is one. */
	void UpdateHeight();

	/** The description, filled in. */
	FText Text;

	/** The count. */
	int32 Current = 0;

	/** The count needed. */
	int32 Required = 1;

	/** Root of the content: sets the row's height. */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SizeBox;
};
