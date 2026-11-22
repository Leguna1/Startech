#pragma once

#include "CoreMinimal.h"
#include "Tiers/SectorTierTypes.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudSectorBannerWidget.generated.h"

class USectorDirectorSubsystem;

/**
 * Top centre: the sector's name and tier ("Rust · Tier II", the tier in its DangerColor from the style), "Sector N · G
 * gates" beneath it, and "Docked · <station>" while docked, with
 * "Press <key> to open <station> station" under it (the key mapped to the ship's interact action, read at runtime).
 * When a sector becomes ready after a warp, a large banner shows for the style's banner time, then settles into the
 * resting layout; during a warp it waits for this element's boot. Event-driven: wrap it in an Invalidation Box.
 */
UCLASS()
class STARTECH_API UHudSectorBannerWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots fifth, after the cargo panel. */
	UHudSectorBannerWidget(const FObjectInitializer& ObjectInitializer);


	/** Plays the arrival banner if a new sector arrived during the warp. */
	virtual void HandleBootStarted() override;

protected:
	/** Binds to the new pawn's docking event. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Reads the current sector and docked state, without playing the banner. */
	virtual void RefreshFromPawn() override;

	/** Binds to the sector director and reads the current sector. */
	virtual void NativeConstruct() override;

	/** Unbinds from the director and the pawn. */
	virtual void NativeDestruct() override;

	/** Repaints while the banner animates. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	UFUNCTION()
	void HandleDockedChanged(bool bDocked, FText StationName);

	/** Reads the name, index and gate count of SectorId from the catalog. */
	void RefreshSector(FName SectorId);

	/** Starts the large banner now. */
	void PlayBanner();

	/** True while the large banner shows. */
	bool IsBannerActive() const;

	/** Director this widget is bound to. */
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;

	/** Sector display name. */
	FText SectorName;

	/** "Tier II" (UStartechTierSubsystem); empty without a tier subsystem. */
	FText TierText;

	/** The style colour TierText is drawn in. */
	ETierColorRole TierColor = ETierColorRole::Text;

	/** "Sector N · G gates". */
	FText SectorLine;

	/** Docked station's name; empty while undocked. */
	FText StationName;

	/** Whether the player is docked. */
	bool bDocked = false;

	/** "Press F to open Terra station" while docked; empty when no key is mapped. */
	FText DockPrompt;

	/** Builds DockPrompt from the bound ship's interact key and StationName. */
	void RefreshDockPrompt();

	/** A sector arrived during a warp; the banner plays at boot. */
	bool bBannerPending = false;

	/** HUD time the large banner started; negative when it isn't showing. */
	double BannerStart = -1.0;
};
