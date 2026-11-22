#pragma once

#include "CoreMinimal.h"
#include "TargetLockState.h"
#include "TargetRangeState.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudReticleWidget.generated.h"

class UHealthComponent;
class UTargetableComponent;
class UTargetingComponent;
class UTargetRangeComponent;

/**
 * Full screen, centred on the screen. With no target: a small circle with two side ticks. With a target: its shield
 * arc on the left and hull arc on the right with percentages, an outer tick ring, and under them the target's name and
 * distance coloured by range state, then the lock label. A soft lock draws thin corner brackets around the target's
 * projected bounds (sliding when the soft target changes); a hard lock draws solid Player-colour brackets on the same
 * frame that close in from 1.6x on engage and expand to 1.4x and fade on release, turns the centre circle Player
 * colour, and plays the style's lock tone. Repaints every frame (it tracks the target),
 * so don't put it in an Invalidation Box.
 */
UCLASS()
class STARTECH_API UHudReticleWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots last after a warp. */
	UHudReticleWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the new pawn's targeting and range components. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads the current target, lock and range state, and the target's health, without snap or tone. */
	virtual void RefreshFromPawn() override;

	/** Unbinds everything. */
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	UFUNCTION()
	void HandleTargetChanged(UTargetableComponent* NewTarget, ETargetLockState NewState);

	UFUNCTION()
	void HandleRangeStateChanged(ETargetRangeState NewState);

	UFUNCTION()
	void HandleTargetShieldChanged(float Current, float Max);

	UFUNCTION()
	void HandleTargetHullChanged(float Current, float Max);

	/** The pulse (0 to 1) of the current target's charged-shot telegraph, or -1 when it isn't charging. */
	float GetTargetChargePulse() const;

	/** Follows NewTarget's health component, dropping the previous one. */
	void BindTargetHealth(const UTargetableComponent* NewTarget);

	/** Unbinds from the targeting, range and target health components. */
	void UnbindAll();

	/** The target's name: its enemy type's display name, else its map marker label, else "Contact". */
	static FText GetTargetName(const AActor* Target);

	/**
	 * The target's projected screen bounds in widget space, grown to at least the style's MinFrameSize. False when
	 * the target is off screen or behind the camera, in which case no frame is drawn.
	 */
	bool GetTargetFrame(const FGeometry& Geometry, FVector2f& OutMin, FVector2f& OutMax) const;

	/** Draws one segmented arc: Segments pieces from StartDegrees, each Step degrees on, Filled of them in FillColor. */
	void DrawSegmentedArc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Centre,
		float StartDegrees, float Step, int32 Filled, const FLinearColor& FillColor, float Thickness, float Opacity) const;

	/** The name and distance colour for the current range state. */
	FLinearColor GetRangeColor() const;

	/** Targeting component of the bound pawn. */
	TWeakObjectPtr<UTargetingComponent> BoundTargeting;

	/** Range component of the bound pawn. */
	TWeakObjectPtr<UTargetRangeComponent> BoundRange;

	/** Health component of the current target. */
	TWeakObjectPtr<UHealthComponent> TargetHealth;

	/** The current target. */
	TWeakObjectPtr<UTargetableComponent> Target;

	/** Lock state of the current target. */
	ETargetLockState LockState = ETargetLockState::None;

	/** Range state against the current target. */
	ETargetRangeState RangeState = ETargetRangeState::OutOfRange;

	/** Cached target shield as a fraction of its maximum. */
	float ShieldFraction = 0.f;

	/** Cached target hull as a fraction of its maximum. */
	float HullFraction = 0.f;

	/** Cached target name. */
	FText TargetName;

	/** HUD time the hard lock engaged, for the snap; negative when none. */
	double HardLockStart = -1.0;

	/** HUD time the hard lock was released, for the expand-and-fade; negative when none. */
	double ReleaseStart = -1.0;

	/** The hard-lock bracket frame as last drawn, where the release animation plays. */
	mutable FVector2f ReleaseFrameMin = FVector2f::ZeroVector;

	/** The hard-lock bracket frame as last drawn. */
	mutable FVector2f ReleaseFrameMax = FVector2f::ZeroVector;

	/** True once a hard-lock frame has been drawn, so a release has somewhere to play. */
	mutable bool bHasReleaseFrame = false;

	/** The lock ring's fade-in (0 to 1) as last drawn, so a release fades it from where it was. */
	mutable float ReleaseRingAlpha = 0.f;

	/**
	 * Draws the optional dotted lock ring circumscribing the bracket frame Min..Max (half its diagonal plus a gap),
	 * rotating with time, at Alpha times the style's ring opacity. Danger colour while in enemy range.
	 */
	void DrawLockRing(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min,
		const FVector2f& Max, float Alpha, double Now) const;

	/** HUD time the soft brackets started sliding to a new target; negative when not sliding. */
	double SlideStart = -1.0;

	/** Bracket frame the slide starts from. */
	FVector2f SlideFromMin = FVector2f::ZeroVector;

	/** Bracket frame the slide starts from. */
	FVector2f SlideFromMax = FVector2f::ZeroVector;

	/** The bracket frame as last drawn, the start of the next slide. */
	mutable FVector2f LastFrameMin = FVector2f::ZeroVector;

	/** The bracket frame as last drawn. */
	mutable FVector2f LastFrameMax = FVector2f::ZeroVector;

	/** False until a frame has been drawn, so the first soft lock doesn't slide in from the corner. */
	mutable bool bHasLastFrame = false;
};
