#pragma once

#include "CoreMinimal.h"
#include "ActionBar/ActionBarTypes.h"
#include "UI/HUD/HudElementWidget.h"
#include "UI/HUD/HudPanelBrush.h"
#include "HudActionBarWidget.generated.h"

class UActionBarComponent;

/**
 * Bottom centre: the action bar. One cut-corner square per slot of the player ship's UActionBarComponent, grouped by
 * label ("Lasers", "Drainer") with the label above each group (the active group's in the Player colour). Each slot
 * shows its key (top left), its count (bottom right, "12.4k" above 9,999, "∞" unlimited) and an icon drawn in code
 * (bars or chevrons by tier) or the item's icon texture. It draws only FActionSlotState and FActionSlotVisual, so it
 * knows no slot kinds:
 *   selected      brighter fill, 2 px Player border, Player underline, Player-tinted icon (border flashes briefly)
 *   low / empty   amber count / red 0 with the slot dimmed (the count flashes once on reaching 0)
 *   out of range  key label in Danger
 *   disabled      whole slot dimmed
 *   cooldown      dark radial sweep with the seconds left, a Player edge flash when ready
 *   active        a thin bar under the slot draining with ActiveFraction
 * Event-driven (UActionBarComponent::OnSlotsChanged); ticks only while a flash, cooldown or active bar animates.
 * Wrap it in an Invalidation Box. Sizes, gaps, opacities and timings come from the style's Action Bar values.
 */
UCLASS()
class STARTECH_API UHudActionBarWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots third, after the radar. */
	UHudActionBarWidget(const FObjectInitializer& ObjectInitializer);

	/** Width the bar's slots take for the current layout and style, in pixels (6 slots in 2 groups: 376). */
	float GetContentWidth() const;

	/** Height the bar takes: group label, slot, underline and active bar, in pixels. */
	float GetContentHeight() const;

	/** Distance from the top of the bar to the middle of its slots, in pixels (for aligning neighbours). */
	float GetSlotCentreY() const;

protected:
	/** Binds to the new pawn's action bar. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads every slot's state, icon and key, without animation. */
	virtual void RefreshFromPawn() override;

	/** Rebuilds the panel material instances. */
	virtual void HandleStyleChanged() override;

	/** Unbinds from the action bar. */
	virtual void NativeDestruct() override;

	/** Repaints while a flash, a cooldown or an active bar animates. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** One slot as drawn. */
	struct FSlotView
	{
		/** Its state. */
		FActionSlotState State;
		/** Its icon. */
		FActionSlotVisual Visual;
		/** Its key label ("1"). */
		FString Key;
		/** Its group label. */
		FText GroupLabel;
		/** Left edge within the bar. */
		float X = 0.f;
		/** HUD time it was last selected, for the border flash; negative for never. */
		double SelectedAt = -1.0;
		/** HUD time its count reached 0, for the count flash; negative for never. */
		double EmptiedAt = -1.0;
		/** HUD time its cooldown ended, for the edge flash; negative for never. */
		double ReadyAt = -1.0;
	};

	/** One group of neighbouring slots with the same label. */
	struct FGroupView
	{
		/** Its label. */
		FText Label;
		/** First slot index. */
		int32 First = 0;
		/** Last slot index. */
		int32 Last = 0;
	};

	/** Action bar callback: re-reads the states and starts the feedback flashes. */
	void HandleSlotsChanged();

	/** Rebuilds Slots and Groups from the action bar; bAnimate starts flashes for changes since the last read. */
	void ReadSlots(bool bAnimate);

	/** Unbinds from the bound action bar. */
	void UnbindBar();

	/** True while anything animates. */
	bool IsAnimating() const;

	/** Draws View's icon centred in the square at Position. */
	void DrawIcon(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FSlotView& View,
		const FVector2f& Position, float Size, const FLinearColor& Color) const;

	/** Draws the dark cooldown sweep over the square at Position, Fraction of a full turn from twelve o'clock. */
	static void DrawCooldownSweep(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
		const FVector2f& Position, float Size, float Fraction, const FLinearColor& Color);

	/** The count as shown: "∞", "487" or "12.4k". */
	static FString FormatCount(int32 Count);

	/** The action bar bound to. */
	TWeakObjectPtr<UActionBarComponent> BoundBar;

	/** The OnSlotsChanged binding. */
	FDelegateHandle SlotsChangedHandle;

	/** Every slot, in bar order. */
	TArray<FSlotView> Slots;

	/** Every group, in bar order. */
	TArray<FGroupView> Groups;

	/** Fill of an unselected slot. */
	UPROPERTY(Transient)
	FHudPanelBrush NormalPanel;

	/** Fill of a selected slot. */
	UPROPERTY(Transient)
	FHudPanelBrush SelectedPanel;
};
