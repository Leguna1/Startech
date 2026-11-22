#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudScreenFeedbackWidget.generated.h"

class AStartechShipPawn;
class UHealthComponent;
class UMaterialInstanceDynamic;
class UWarpTransitionComponent;

/**
 * Full screen, above everything: the edge effects and warnings.
 * - Shield hit: a Shield-colour edge ripple on the side the damage came from (ahead is the top edge), or all edges
 *   for a hit with no source, fading over the style's hit time.
 * - Hull hit: a short Danger flash on all edges.
 * - Low hull: a pulsing "Hull critical" just above the reticle text.
 * - Out of bounds: a Danger edge tint pulsing between the style's strengths, faster over the last seconds of the
 *   countdown, and "Return to the sector. Hull destroyed in N s".
 * - First person: a thin cut-corner cockpit-glass frame inset from the screen edges.
 * - Warp refused: the refusal reason below the reticle text, with the seconds left in combat
 *   ("Can't warp while in combat · 3 s"), while the gate retries the warp, until it starts or the ship leaves.
 * - Death: while the screen is black, a centred "Ship destroyed. Returning to <home>." ("Ship destroyed." when dying
 *   at home) and, if cargo was dropped, "Your cargo remains in <sector> for m:ss" counting down. It fades with the
 *   camera fade, and this element stays up while the rest of the HUD is folded for the death.
 * - Charged shot: while an enemy charges a shot at the player, a pulsing Danger bracket on it when it is on screen
 *   (the current target included; the reticle also tints its own brackets), or a pulsing Danger edge towards it when it
 *   is off screen. The pulse follows the charge's own (UEnemyChargeComponent::GetPulse).
 * Edge effects use M_HUD_EdgeFeedback (one dynamic instance each); without it they fall back to plain edge bars.
 */
UCLASS()
class STARTECH_API UHudScreenFeedbackWidget : public UHudElementWidget
{
	GENERATED_BODY()

protected:
	/** Binds to the new pawn's health and ship events. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads the low-hull, out-of-bounds and camera state. */
	virtual void RefreshFromPawn() override;

	/** Unbinds everything. */
	virtual void NativeDestruct() override;

	/** Advances the out-of-bounds pulse. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Rebuilds the edge material instances. */
	virtual void HandleStyleChanged() override;

	/** The death message is drawn by this element, so it stays up while the rest of the HUD folds. */
	virtual bool StaysUpWhileRespawning() const override { return true; }

private:
	UFUNCTION()
	void HandleDamageTaken(float Amount, bool bHitShield, FVector SourceLocation);

	UFUNCTION()
	void HandleLowHullChanged(bool bIsLow);

	UFUNCTION()
	void HandleOutOfBoundsChanged(bool bOutside);

	UFUNCTION()
	void HandleCameraModeChanged(bool bFirstPerson);

	/** Warp callback: shows the refusal reason. */
	UFUNCTION()
	void HandleWarpRefused(FText Reason);

	/** Warp callback: a warp started, so any refusal message goes. */
	UFUNCTION()
	void HandleWarpStarted(FName DestinationSectorId);

	/** True while a warp refusal is to be shown: the gate is retrying it, or it was refused moments ago. */
	bool IsWarpRefusalShown() const;

	/** Draws the warp refusal line below the reticle text. */
	void DrawWarpRefusal(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry) const;

	/** The screen direction toward SourceLocation (ahead is up), or zero when the hit has no source. */
	FVector2f GetHitDirection(const FVector& SourceLocation) const;

	/** Draws one edge effect: through its material instance if there is one, else as plain edge bars. */
	void DrawEdge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, UMaterialInstanceDynamic* Instance,
		const FSlateBrush& Brush, const FLinearColor& Color, float Intensity, const FVector2f& Direction) const;

	/** The charged-shot warnings: brackets on charging enemies on screen, the edge towards one off screen. */
	void DrawChargeWarnings(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry) const;

	/** Actor's projected bounds in widget space; false when it isn't fully in front of the camera and on screen. */
	bool GetActorFrame(const AActor& Actor, const FGeometry& Geometry, FVector2f& OutMin, FVector2f& OutMax) const;

	/** Out-of-bounds pulse rate now, in Hz: the style's rate, rising to the urgent rate near the end of the countdown. */
	float GetOutOfBoundsPulseRate() const;

	/** Makes a dynamic instance of the style's edge material, or null without one. */
	UMaterialInstanceDynamic* MakeEdgeInstance();

	/** Unbinds from the bound health component and ship. */
	void UnbindAll();

	/** Draws the death message centred on the screen, at the camera fade's opacity, while the player is respawning. */
	void DrawDeathMessage(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry) const;

	/** The owning player's camera fade amount: 0 clear, 1 fully black. */
	float GetScreenCover() const;

	TWeakObjectPtr<UHealthComponent> BoundHealth;
	TWeakObjectPtr<AStartechShipPawn> BoundShip;

	/** The bound ship's warp component, for refusals. */
	TWeakObjectPtr<UWarpTransitionComponent> BoundWarp;

	/** Reason of the last warp refusal; empty when none is shown. */
	FText WarpRefusalReason;

	/** HUD time of the last warp refusal. */
	double WarpRefusalTime = -1.0;

	/** Edge instance for the shield ripple. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldEdge;

	/** Edge instance for the hull flash. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HullEdge;

	/** Edge instance for the out-of-bounds tint. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoundsEdge;

	/** Edge instance for the charged-shot warning. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ChargeEdge;

	/** Brush drawing ChargeEdge. */
	FSlateBrush ChargeBrush;

	/** Brush drawing ShieldEdge. */
	FSlateBrush ShieldBrush;

	/** Brush drawing HullEdge. */
	FSlateBrush HullBrush;

	/** Brush drawing BoundsEdge. */
	FSlateBrush BoundsBrush;

	/** HUD time of the last shield hit; negative for none. */
	double ShieldHitTime = -1.0;

	/** Screen direction of the last shield hit; zero for all edges. */
	FVector2f ShieldHitDirection = FVector2f::ZeroVector;

	/** HUD time of the last hull hit; negative for none. */
	double HullHitTime = -1.0;

	/** Out-of-bounds pulse phase in turns (0 to 1), integrated so a rising rate doesn't jump. */
	float BoundsPulsePhase = 0.f;

	bool bLowHull = false;
	bool bOutOfBounds = false;
	bool bFirstPerson = false;
};
