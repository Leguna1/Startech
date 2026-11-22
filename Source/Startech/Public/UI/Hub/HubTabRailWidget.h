#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.h"
#include "UI/Hub/HubTab.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubTabRailWidget.generated.h"

class UQuestSubsystem;

/** Broadcast when the player clicks an available tab in the rail. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHubTabSelected, EHubTab);

/**
 * The hub's tab rail, painted in C++: Quests (with a gold count of quests ready to turn in), Shop and Upgrades, then
 * Ship log and Achievements greyed out and not clickable, with "Coming later" under them. The selected tab gets the
 * Panel fill and a 3 px Player-colour bar on its left edge. The ready count follows the quest status events.
 */
UCLASS()
class STARTECH_API UHubTabRailWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse. */
	UHubTabRailWidget(const FObjectInitializer& ObjectInitializer);

	/** Highlights Tab, without broadcasting. */
	void SetSelectedTab(EHubTab Tab);

	/** Fires when the player clicks an available tab other than the selected one. */
	FOnHubTabSelected OnTabSelected;

protected:
	/** A size box giving the rail its width. */
	virtual void BuildContent() override;

	/** Binds to the quest events and reads the ready count. */
	virtual void NativeConstruct() override;

	/** Unbinds from the quest events. */
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	/** A status changed: re-counts the ready quests. */
	UFUNCTION()
	void HandleQuestStatusChanged(FName QuestId, EQuestStatus NewStatus);

	/** A quest left the registry: re-counts the ready quests. */
	UFUNCTION()
	void HandleQuestUnregistered(FName QuestId);

	/** Counts the ReadyToTurnIn quests and repaints. */
	void RefreshReadyCount();

	/** Top of the tab row Index (0 Quests, 1 Shop, 2 Upgrades, 3-4 the later tabs), in local units. */
	float GetRowTop(int32 Index) const;

	/** The row under the local Position, or INDEX_NONE. */
	int32 HitTest(const FVector2f& Position) const;

	/** The selected tab. */
	EHubTab Selected = EHubTab::Quests;

	/** The row under the mouse, or INDEX_NONE. */
	int32 HoveredRow = INDEX_NONE;

	/** Quests ready to turn in. */
	int32 ReadyCount = 0;

	/** The quest subsystem bound to. */
	TWeakObjectPtr<UQuestSubsystem> BoundQuests;
};
