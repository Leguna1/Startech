#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.h"
#include "UI/HUD/HudElementWidget.h"
#include "UI/HUD/HudPanelBrush.h"
#include "HudQuestTrackerWidget.generated.h"

class AStartechShipPawn;
class UQuestSubsystem;

/**
 * Top right: every active quest, at most the style's TrackerMaxEntries, timed first, then story, then bounties.
 * Each entry shows the title (12 pt) with a right-aligned timer for timed quests (amber below 30 s, red below 10 s),
 * and one line for the current objective (the first incomplete one), or "Ready to turn in at <station>" in the
 * Advantage colour. Draws nothing when no quest is active. Folds and boots with the rest of the HUD.
 * Event-driven: it repaints only on quest events (the timer once per second, from OnQuestTimeChanged), so it can sit
 * in an Invalidation Box.
 */
UCLASS()
class STARTECH_API UHudQuestTrackerWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots with the sector banner. */
	UHudQuestTrackerWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the quest subsystem's events. */
	virtual void NativeConstruct() override;

	/** Unbinds from the quest subsystem and the ship. */
	virtual void NativeDestruct() override;

	/** Makes the panel material instance. */
	virtual void HandleStyleChanged() override;

	/** Binds to the ship's docking event, for the station's name. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Re-reads every active quest. */
	virtual void RefreshFromPawn() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** One tracked quest, as drawn. */
	struct FTrackerEntry
	{
		/** The quest. */
		FName QuestId;
		/** Its title. */
		FString Title;
		/** "m:ss" for a running clock; empty otherwise. */
		FString Timer;
		/** Colour of Timer. */
		FLinearColor TimerColor = FLinearColor::White;
		/** The current objective, or the ready line. */
		FString Line;
		/** Colour of Line. */
		FLinearColor LineColor = FLinearColor::White;
	};

	UFUNCTION()
	void HandleQuestStatusChanged(FName QuestId, EQuestStatus NewStatus);

	UFUNCTION()
	void HandleObjectiveProgress(FName QuestId, int32 ObjectiveIndex, int32 Current, int32 Required);

	UFUNCTION()
	void HandleQuestTimeChanged(FName QuestId, float SecondsRemaining);

	UFUNCTION()
	void HandleQuestUnregistered(FName QuestId);

	UFUNCTION()
	void HandleDockedChanged(bool bDocked, FText InStationName);

	/** Rebuilds Entries from the quest subsystem and repaints. */
	void RebuildEntries();

	/** Fills Entry's timer and line from the quest's current state. */
	void FillEntry(FTrackerEntry& Entry) const;

	/** The station to turn in at: the last docked station's name, else the home sector's name. */
	FText GetTurnInPlace() const;

	/** The quest subsystem bound to. */
	TWeakObjectPtr<UQuestSubsystem> BoundQuests;

	/** The ship bound to, for its docking event. */
	TWeakObjectPtr<AStartechShipPawn> BoundShip;

	/** The name of the station the player last docked at; empty until then. */
	FText StationName;

	/** The tracked quests, in display order. */
	TArray<FTrackerEntry> Entries;

	/** The panel behind the entries. */
	UPROPERTY(Transient)
	FHudPanelBrush Panel;
};
