#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "QuestListRowWidget.generated.h"

class USizeBox;

/** Broadcast when a quest row is clicked, with the row's quest id. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestRowClicked, FName);

/**
 * One quest in the hub's quest list, painted in C++: the title (12 pt) with its state on the right ("Ready" in the
 * Advantage colour, a running timer in m:ss turning amber then red, a bounty's reward in the Credits colour, or a
 * cooldown), and a short summary underneath in secondary text. The selected row gets a Player-colour border.
 */
UCLASS()
class STARTECH_API UQuestListRowWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse. */
	UQuestListRowWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows QuestId and reads its current state. */
	void Setup(FName InQuestId);

	/** Re-reads the quest's title, state and summary from the quest subsystem, and repaints. */
	void Refresh();

	/** Draws or removes the selection border. */
	void SetSelected(bool bInSelected);

	/** The quest shown. */
	FName GetQuestId() const { return QuestId; }

	/** True while the row shows a cooldown, which has no event of its own and is refreshed by the list's timer. */
	bool IsCoolingDown() const { return bCoolingDown; }

	/** Fires when the row is clicked. */
	FOnQuestRowClicked OnClicked;

protected:
	/** A size box giving the row its height. */
	virtual void BuildContent() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	/** The quest shown. */
	FName QuestId;

	/** Its title. */
	FText Title;

	/** The right-hand state text; may be empty. */
	FText StateText;

	/** Colour of StateText. */
	FLinearColor StateColor = FLinearColor::White;

	/** The line under the title. */
	FText Summary;

	/** Selected in the list. */
	bool bSelected = false;

	/** The mouse is over the row. */
	bool bHovered = false;

	/** The quest is waiting out a cooldown. */
	bool bCoolingDown = false;
};
