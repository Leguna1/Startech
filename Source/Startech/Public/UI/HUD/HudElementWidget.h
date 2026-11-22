#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HudElementWidget.generated.h"

class APawn;
class UStartechHudStyle;

/**
 * Base of every HUD element. An element draws its own content in NativePaint from values it caches from gameplay
 * events, and repaints only when they change (or every frame, for the few that track moving things).
 * The root UStartechHudWidget hands it the style, the player pawn to bind to, and the warp fold and boot.
 */
UCLASS(Abstract)
class STARTECH_API UHudElementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Display only: never takes the mouse. */
	UHudElementWidget(const FObjectInitializer& ObjectInitializer);

	/** The style to use when this element has none of its own. Set by the root. */
	void SetInheritedStyle(const UStartechHudStyle* InStyle);

	/** This element's style, else the inherited one, else the style class defaults. Never null. */
	const UStartechHudStyle& GetHudStyle() const;

	/**
	 * Called by the root when the player pawn changes (either may be null): HandlePawnChanged moves the bindings,
	 * then SyncToPawn shows the new pawn's current values at once.
	 */
	void BindToPawn(APawn* OldPawn, APawn* NewPawn);

	/**
	 * Re-reads everything this element shows and applies it immediately, with no animation (no segment flicker,
	 * count-up or lock snap), then repaints. Called on binding, on a style change, and by the root after a respawn
	 * or a warp, so no element depends on a first event to be correct.
	 */
	void SyncToPawn();

	/** 0 shown, 1 folded away: fades the element and shrinks it toward its anchor. */
	void SetFoldAmount(float Folded);

	/** Current fold amount, 0 shown to 1 folded. */
	float GetFoldAmount() const { return FoldAmount; }

	/** Called by the root as this element's boot-up starts after a warp. */
	virtual void HandleBootStarted() {}

	/** Lower boots first after a warp; equal orders boot together. */
	int32 GetBootOrder() const { return BootOrder; }

	/** True for an element that stays shown while the rest of the HUD is folded away for the player's death. */
	virtual bool StaysUpWhileRespawning() const { return false; }

protected:
	virtual void NativeConstruct() override;

	/** Called when the resolved style changes, so elements can rebuild material instances. */
	virtual void HandleStyleChanged() {}

	/**
	 * Unbind from OldPawn's components and bind to NewPawn's. Either may be null. Only moves bindings: the values
	 * are read afterwards in RefreshFromPawn. Overrides must call Super, which stores the bound pawn.
	 */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	/**
	 * Reads every displayed value from the bound pawn's components (and from subsystems) with their getters and
	 * applies it without animation. Must cope with no bound pawn, and with components that haven't begun play yet.
	 */
	virtual void RefreshFromPawn() {}

	/** The pawn this element is bound to, or null. */
	APawn* GetBoundPawn() const { return BoundPawn.Get(); }

	/** Marks the element for repaint. Inside an Invalidation Box this is what redraws it. */
	void RequestRepaint() { Invalidate(EInvalidateWidgetReason::Paint); }

	/** Frame time in seconds, for animations. */
	static double GetHudTime();

	/** Optional: overrides the style the root hands down, for this element only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UStartechHudStyle> Style;

	/** Lower boots first after a warp; equal orders boot together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	int32 BootOrder = 5;

private:
	/** The widget positioned in the root's canvas: this element, or the Invalidation Box wrapping it. */
	UWidget* GetLayoutWidget();

	/** Style handed down by the root. */
	TWeakObjectPtr<const UStartechHudStyle> InheritedStyle;

	/** The pawn this element is bound to. */
	TWeakObjectPtr<APawn> BoundPawn;

	/** Current fold amount. */
	float FoldAmount = 0.f;
};
