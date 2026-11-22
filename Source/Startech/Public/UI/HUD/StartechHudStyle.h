#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Tiers/SectorTierTypes.h"
#include "StartechHudStyle.generated.h"

class UMaterialInterface;
class USoundBase;

/**
 * Every visual constant of the HUD, so the look is tuned in one place. The defaults are the reference values;
 * DA_HudStyle (made by Tools/HUD/create_hud_style.py) holds the same values plus the two HUD materials.
 * Each colour has one fixed meaning and is never reused for another purpose.
 */
UCLASS(BlueprintType)
class STARTECH_API UStartechHudStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The style widgets fall back to when none is assigned: this class's defaults. Never null. */
	static const UStartechHudStyle& GetDefaultStyle();

	/** The engine's Roboto at Size points, bold or regular. No font assets needed. */
	static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);

	/** Hex sRGB to the linear colour Slate expects, with Alpha in 0-1. */
	static FLinearColor FromHex(const TCHAR* Hex, float Alpha = 1.f);

	/** This style's colour for Role (the sector tier colours): TextColor, CreditsColor, HullColor or DangerColor. */
	FLinearColor GetTierColor(ETierColorRole Role) const;

	// Colours

	/** You: player card, hard lock, the player's radar marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor PlayerColor = FromHex(TEXT("5DCAA5"));

	/** Shields, yours and targets'. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor ShieldColor = FromHex(TEXT("85B7EB"));

	/** Hull, yours and targets'. Also the cargo bar's nearly-full warning and low ammo. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor HullColor = FromHex(TEXT("EF9F27"));

	/** Danger only: inside the enemy's range, low hull, out of bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor DangerColor = FromHex(TEXT("E24B4A"));

	/** Danger, for text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor DangerTextColor = FromHex(TEXT("F09595"));

	/** Player advantage range, item gains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor AdvantageColor = FromHex(TEXT("97C459"));

	/** Credits; gate and death-cargo markers and loot on the radar use it too. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor CreditsColor = FromHex(TEXT("FAC775"));

	/** Labels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor TextColor = FromHex(TEXT("D3D1C7"));

	/** Secondary labels, neutral radar contacts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor TextSecondaryColor = FromHex(TEXT("888780"));

	/** Unfilled gauge segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor EmptySegmentColor = FromHex(TEXT("2C2C2A"));

	/** Panel backgrounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor PanelFillColor = FromHex(TEXT("042C53"), 0.35f);

	/** Soft-lock brackets and label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colours")
	FLinearColor SoftLockColor = FromHex(TEXT("B4B2A9"));

	// Gauges

	/** Segments in the player card's shield and hull bars and in the reticle's arcs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauges", meta = (ClampMin = "1"))
	int32 HealthSegments = 10;

	/** Segments in the cargo bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauges", meta = (ClampMin = "1"))
	int32 CargoSegments = 10;

	/** Fill fraction at which the cargo bar turns Hull colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauges", meta = (ClampMin = "0", ClampMax = "1"))
	float CargoWarnFraction = 0.8f;

	/** Rounds below which the ammo count turns Hull colour. At 0 it is Danger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauges", meta = (ClampMin = "0"))
	int32 LowAmmoThreshold = 100;

	/** Most segments in the ammo bar (shown only once ammo has a capacity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauges", meta = (ClampMin = "1"))
	int32 MaxAmmoSegments = 20;

	/** Segments in the boost gauge under the speed number. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauges", meta = (ClampMin = "1"))
	int32 BoostSegments = 10;

	// Timings, in seconds

	/** A lost gauge segment flickers this long before staying empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float SegmentFlickerTime = 0.25f;

	/** Flicker rate while a segment flickers, in toggles per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "1"))
	float SegmentFlickerRate = 24.f;

	/** The boost gauge flashes Danger this long when the boost key is pressed with too little energy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float BoostDeniedFlashTime = 0.35f;

	/** Credits count to a new total over this time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float CreditsCountTime = 0.6f;

	/** Each pickup feed line lasts this long, fading out at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float PickupLineLifetime = 3.f;

	/** Most pickup feed lines on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "1"))
	int32 PickupMaxLines = 4;

	/** Hard-lock diamond snap from 2x to 1x, soft bracket slide, and release fade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float LockSnapTime = 0.15f;

	/** Each element folds away over this time when a warp starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float FoldTime = 0.3f;

	/** Each element boots back over this time after arrival. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float BootTime = 0.6f;

	/** Delay between one element's boot and the next. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float BootStagger = 0.08f;

	/** Folded elements shrink to this scale toward their anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", ClampMax = "1"))
	float FoldedScale = 0.6f;

	/** The large sector banner shows this long after arrival. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float SectorBannerTime = 2.5f;

	/** Shield ripple and hull flash fade over this time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float HitEffectTime = 0.4f;

	/** Credits text tint after a gain or loss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0", Units = "s"))
	float CreditsTintTime = 0.8f;

	/** Pulses per second of "Hull critical" and the low hull bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timings", meta = (ClampMin = "0"))
	float LowHullPulseRate = 1.5f;

	// Fonts (point sizes of the engine's Roboto)

	/** Small labels: bar captions, secondary lines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (ClampMin = "4"))
	int32 SmallFontSize = 9;

	/** Body text: callsign, target name, ammo name, warnings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (ClampMin = "4"))
	int32 BodyFontSize = 12;

	/** Sector name, ammo count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (ClampMin = "4"))
	int32 HeadingFontSize = 14;

	/** Credits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (ClampMin = "4"))
	int32 CreditsFontSize = 16;

	/** The large arrival banner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts", meta = (ClampMin = "4"))
	int32 BannerFontSize = 24;

	// Reticle, in pixels

	/** Radius of the target's shield and hull arcs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "1"))
	float ArcRadius = 46.f;

	/** Radius of the outer tick ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "1"))
	float TickRingRadius = 58.f;

	/** Arc line thickness under a hard lock; a soft lock uses 60% of it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "0.5"))
	float ArcThickness = 4.f;

	/** Smallest side of the soft-lock bracket frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "1"))
	float MinFrameSize = 24.f;

	/** Distance below the reticle centre of the target name line; "Hull critical" sits just above it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "0"))
	float ReticleTextOffset = 80.f;

	/** Played when a hard lock engages. May be empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle")
	TObjectPtr<USoundBase> LockTone;

	/**
	 * Draws a slowly rotating dotted ring around the hard-lock brackets. It fades in once the brackets have closed,
	 * and turns Danger colour while the target can hit the player.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle")
	bool bShowLockRing = false;

	/** Seconds for one full turn of the lock ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "0.1", Units = "s", EditCondition = "bShowLockRing"))
	float LockRingSecondsPerTurn = 18.f;

	/** Opacity of the lock ring's dots once fully faded in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bShowLockRing"))
	float LockRingOpacity = 0.5f;

	/** Number of dots around the lock ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reticle", meta = (ClampMin = "3", EditCondition = "bShowLockRing"))
	int32 LockRingDotCount = 24;

	// Markers

	/** World markers closer than this are hidden so they don't sit on the object, in cm (300 m). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markers", meta = (ClampMin = "0"))
	float MarkerHideDistance = 30000.f;

	/** Side of a world marker's hollow diamond, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Markers", meta = (ClampMin = "1"))
	float MarkerSize = 12.f;

	// Panels and feedback

	/** M_HUD_ChamferPanel. Without it panels fall back to a plain box with a drawn outline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> PanelMaterial;

	/** M_HUD_EdgeFeedback. Without it screen feedback falls back to plain edge bars. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Materials")
	TObjectPtr<UMaterialInterface> EdgeFeedbackMaterial;

	/** Cut size of panel corners, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels", meta = (ClampMin = "0"))
	float PanelCornerSize = 10.f;

	/** Panel outline width, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels", meta = (ClampMin = "0"))
	float PanelBorderWidth = 1.f;

	/** Edge glow width as a fraction of half the screen height; the same thickness on every edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.01", ClampMax = "1"))
	float EdgeGlowWidth = 0.18f;

	/** Out of bounds: the edge tint pulses between this strength... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0", ClampMax = "1"))
	float OutOfBoundsPulseMin = 0.15f;

	/** ...and this one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0", ClampMax = "1"))
	float OutOfBoundsPulseMax = 0.7f;

	/** Pulses per second of the out-of-bounds tint while there is still time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0", Units = "Hz"))
	float OutOfBoundsPulseRate = 1.f;

	/** Pulses per second at the end of the countdown; the rate rises to it over OutOfBoundsUrgentTime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0", Units = "Hz"))
	float OutOfBoundsPulseRateUrgent = 3.f;

	/** Seconds left on the out-of-bounds countdown when the pulse starts speeding up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0", Units = "s"))
	float OutOfBoundsUrgentTime = 3.f;

	/** Inset of the cockpit-glass frame from the screen edges, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0"))
	float CockpitFrameInset = 24.f;

	/** Opacity of the cockpit-glass frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0", ClampMax = "1"))
	float CockpitFrameOpacity = 0.3f;

	// Action bar (UHudActionBarWidget). The low-ammo threshold is LowAmmoThreshold above.

	/** Side of one square slot, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "16"))
	float ActionSlotSize = 56.f;

	/** Gap between two slots of the same group, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0"))
	float ActionSlotGap = 6.f;

	/** Gap between two groups, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0"))
	float ActionGroupGap = 16.f;

	/** Size of the group labels, key labels and counts, in points. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "4"))
	int32 ActionTextFontSize = 11;

	/** Opacity of a slot's panel fill (the panel fill colour's own alpha is replaced). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionFillOpacity = 0.25f;

	/** Opacity of the selected slot's panel fill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionSelectedFillOpacity = 0.45f;

	/** The selected slot's border width, in pixels (others draw 1 px). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "1"))
	float ActionSelectedBorderWidth = 2.f;

	/** Height of the Player-colour underline under the selected slot, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "1"))
	float ActionUnderlineHeight = 3.f;

	/** Opacity of an empty slot and its icon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionEmptyOpacity = 0.55f;

	/** Opacity of a disabled slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionDisabledOpacity = 0.4f;

	/** Opacity of the dark cooldown sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", ClampMax = "1"))
	float ActionCooldownOpacity = 0.6f;

	/** Seconds the border stays brightened after a slot is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", Units = "s"))
	float ActionSelectFlashTime = 0.15f;

	/** Seconds the count flashes in Danger when it reaches 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", Units = "s"))
	float ActionEmptyFlashTime = 0.4f;

	/** Seconds the Player-colour edge flashes when a cooldown ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (ClampMin = "0", Units = "s"))
	float ActionReadyFlashTime = 0.3f;

	// Station hub

	/** Full-screen backdrop behind the station hub, over the game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor HubBackdropColor = FromHex(TEXT("020A14"), 0.9f);

	/** Width of the hub's tab rail, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "40"))
	float HubTabRailWidth = 110.f;

	/** Height of one tab in the rail, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "16"))
	float HubTabHeight = 40.f;

	/** Height of one quest row in the hub's quest list, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "16"))
	float HubListRowHeight = 48.f;

	/** Height of one objective line in the quest details, without its progress bar, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "12"))
	float HubObjectiveRowHeight = 22.f;

	/** Height of the progress bar under an objective that counts above 1, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "2"))
	float HubObjectiveBarHeight = 6.f;

	/** Segments in an objective's progress bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "1"))
	int32 HubObjectiveSegments = 10;

	/** Height of a hub button, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "16"))
	float HubButtonHeight = 34.f;

	/** Space left and right of a hub button's label, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "0"))
	float HubButtonPadding = 18.f;

	/** Fill of a cut-corner button at rest. Its outline is the Player colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor ButtonFillColor = FromHex(TEXT("5DCAA5"), 0.22f);

	/** Fill of a cut-corner button under the mouse: brighter than at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor ButtonHoverFillColor = FromHex(TEXT("5DCAA5"), 0.4f);

	/** Fill of a cut-corner button while held down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor ButtonPressedFillColor = FromHex(TEXT("5DCAA5"), 0.58f);

	/**
	 * Fill of a Secondary (and unselected Toggle) button at rest: quieter than a Primary one. Under the mouse it takes
	 * ButtonFillColor, and while held ButtonHoverFillColor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor SecondaryButtonFillColor = FromHex(TEXT("042C53"), 0.55f);

	/** Outline of a Secondary (and unselected Toggle) button. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor SecondaryButtonBorderColor = FromHex(TEXT("888780"), 0.6f);

	/** Fill of a selected Toggle button (a quantity chip, a shop tab). Its outline is the Player colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	FLinearColor ButtonSelectedFillColor = FromHex(TEXT("5DCAA5"), 0.5f);

	/** Opacity of a disabled button, and of the tabs that are not available yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "0", ClampMax = "1"))
	float ButtonDisabledOpacity = 0.4f;

	/** Group headers in the quest list ("Active", "Bounty board"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "4"))
	int32 HubGroupFontSize = 11;

	/** Quest title in the quest details. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "4"))
	int32 HubTitleFontSize = 16;

	/** A quest timer turns Hull colour (amber) below this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "0", Units = "s"))
	float QuestTimerWarnSeconds = 30.f;

	/** A quest timer turns Danger colour below this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub", meta = (ClampMin = "0", Units = "s"))
	float QuestTimerCriticalSeconds = 10.f;

	// HUD quest tracker and notices

	/** Most quests in the HUD quest tracker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests", meta = (ClampMin = "1"))
	int32 TrackerMaxEntries = 5;

	/** Each quest notice lasts this long, fading out at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests", meta = (ClampMin = "0", Units = "s"))
	float NoticeLifetime = 4.f;

	/** Most quest notices stacked at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests", meta = (ClampMin = "1"))
	int32 NoticeMaxLines = 3;
};
