#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudSpeedReadoutWidget.generated.h"

class UShipBoostComponent;

/**
 * The speed number: the player ship's GetSpeed() in m/s, rounded, as "320 m/s" in secondary text, right-aligned. Under
 * it, the boost gauge: BoostSegments segments in the Player colour, Hull (amber) while the energy is below the restart
 * threshold, and one short Danger flash when the boost key is pressed with too little energy.
 * The speed is read every frame, so keep it outside any Invalidation Box; the gauge follows the boost's events.
 * Placed right of the action bar.
 */
UCLASS()
class STARTECH_API UHudSpeedReadoutWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots with the action bar. */
	UHudSpeedReadoutWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the new pawn's boost. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads the boost energy, maximum and restart threshold. */
	virtual void RefreshFromPawn() override;

	/** Unbinds from the boost. */
	virtual void NativeDestruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	UFUNCTION()
	void HandleBoostEnergyChanged(float Current, float Max);

	UFUNCTION()
	void HandleBoostDenied();

	/** Unbinds from the bound boost. */
	void UnbindBoost();

	/** The boost this widget follows. */
	TWeakObjectPtr<UShipBoostComponent> BoundBoost;

	/** Energy as last reported. */
	float Energy = 0.f;

	/** Maximum energy as last reported. */
	float MaxEnergy = 0.f;

	/** Below this the gauge is amber. */
	float RestartThreshold = 0.f;

	/** HUD time of the last refused press; negative for none. */
	double DeniedTime = -1.0;
};
