#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartechHudWidget.generated.h"

class AController;
class APawn;
class AStartechShipPawn;
class UHudElementWidget;
class UHomeRespawnSubsystem;
class UStartechHudStyle;
class UWarpTransitionComponent;

/**
 * The C++ parent of WB_GameHUD, the HUD's root. WB_GameHUD only lays out the elements in a Canvas Panel; this class
 * finds every UHudElementWidget in it, hands them the style, binds them to the player pawn (rebinding when the pawn
 * changes), and runs the warp fold and boot: on OnWarpStarted every element folds toward its anchor over the style's
 * fold time; on OnWarpFinished they boot back in BootOrder, each group the style's stagger after the previous.
 * The player's death folds every element except those that stay up while respawning (the death message), and the
 * respawn boots them back; a warp that ends during the respawn does not boot them.
 * Every element shows the pawn's current values as soon as it is bound, and is re-synced after a respawn and a warp,
 * so nothing waits for a first gameplay event.
 * While the station hub is open the whole HUD is collapsed (kept, not destroyed: its elements stay bound and keep
 * their values), and its visibility is restored when the hub closes.
 */
UCLASS()
class STARTECH_API UStartechHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Finds the elements, hands down the style and binds to the owning player's pawn. */
	virtual void NativeConstruct() override;

	/** Unbinds from the player controller, the pawn and its warp component. */
	virtual void NativeDestruct() override;

	/** Advances the fold and boot animations. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The HUD's look. Handed to every element that has none of its own. Set it to DA_HudStyle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UStartechHudStyle> Style;

private:
	/** One element's fold animation. */
	struct FElementAnim
	{
		/** The element. */
		TWeakObjectPtr<UHudElementWidget> Element;
		/** Fold amount the animation starts from. */
		float From = 0.f;
		/** Fold amount it ends at: 1 folded, 0 shown. */
		float To = 0.f;
		/** HUD time it starts; may be in the future for a staggered boot. */
		double Start = 0.0;
		/** Seconds it takes. */
		float Duration = 0.f;
		/** True while it has to run. */
		bool bActive = false;
		/** For a boot: true once HandleBootStarted has been called. */
		bool bBootNotified = true;
	};

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleWarpStarted(FName DestinationSectorId);

	UFUNCTION()
	void HandleWarpFinished(FName SectorId);

	/** Collapses the HUD while the station hub is open and restores it when the hub closes. */
	UFUNCTION()
	void HandleHubOpenChanged(bool bOpen);

	/** Re-syncs every element once the ship is back after death. */
	UFUNCTION()
	void HandlePlayerRespawned();

	/** Folds the HUD when the player's death sequence starts, and boots it once the ship has respawned. */
	UFUNCTION()
	void HandleRespawnSequenceChanged(bool bActive);

	/** True while the player's death-and-respawn sequence runs. */
	bool IsRespawning() const;

	/** Makes every element re-read its values from the pawn and show them without animation. */
	void SyncElements();

	/** Rebinds every element and the warp component from the current pawn to NewPawn. */
	void SetPawn(APawn* NewPawn);

	/** Collapses the HUD, or restores the visibility it had before. */
	void SetHiddenForHub(bool bHidden);

	/** Collects the elements in the widget tree. */
	void GatherElements();

	/** Starts folding every element away; for a death, elements that stay up while respawning are shown instead. */
	void StartFold(bool bForDeath = false);

	/** Starts the staggered boot of every element. */
	void StartBoot();

	/** The pawn the elements are bound to. */
	TWeakObjectPtr<APawn> BoundPawn;

	/** That pawn's warp component (added in BP_Ship). */
	TWeakObjectPtr<UWarpTransitionComponent> BoundWarp;

	/** That pawn as the player ship, for its respawn event; null for any other pawn. */
	TWeakObjectPtr<AStartechShipPawn> BoundShip;

	/** The HUD's visibility before the station hub collapsed it. */
	ESlateVisibility VisibilityBeforeHub = ESlateVisibility::SelfHitTestInvisible;

	/** True while the HUD is collapsed for the station hub. */
	bool bHiddenForHub = false;

	/** Every element, with its fold animation. */
	TArray<FElementAnim> Elements;

	/** The controller whose possession changes are followed. */
	TWeakObjectPtr<AController> BoundController;

	/** The world's respawn subsystem, for the death fold and the respawn boot. */
	TWeakObjectPtr<UHomeRespawnSubsystem> BoundRespawn;
};
