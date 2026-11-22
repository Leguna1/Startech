#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudQuestNoticeWidget.generated.h"

class AStartechShipPawn;
class UQuestSubsystem;

/**
 * Below the quest tracker: short right-aligned quest notices, newest at the top, at most the style's NoticeMaxLines,
 * each lasting NoticeLifetime and fading out at the end:
 * - "Objective complete: Visit Rust" when an objective reaches its count;
 * - "Ready to turn in: First contract" and "Quest complete: First contract" in the Advantage colour;
 * - "Quest failed: Drifter sweep" in Danger;
 * - the first visit to a higher sector tier: the tier's FirstVisitNotice ("Tier III sector: enemies are much
 *   stronger") in its NoticeColor (UStartechTierSubsystem::OnFirstVisitToTier);
 * - "Hull plating upgraded to level 3" in the Player colour (UShipUpgradeSubsystem::OnUpgradeChanged; bought in the hub,
 *   so it shows once the hub closes);
 * - "LL2 empty, switched to LL1" in the Hull colour when the selected laser ammo runs out while firing.
 * No reward notices: turn-in rewards already show in the pickup feed through the credits and cargo events.
 * While the station hub is open the notices are hidden and held: those on screen and those that arrive meanwhile
 * show, each with its full lifetime, once the hub closes.
 */
UCLASS()
class STARTECH_API UHudQuestNoticeWidget : public UHudElementWidget
{
	GENERATED_BODY()

public:
	/** Boots with the quest tracker. */
	UHudQuestNoticeWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** Binds to the quest subsystem's events and remembers the active quests' titles. */
	virtual void NativeConstruct() override;

	/** Unbinds from the quest subsystem and the ship. */
	virtual void NativeDestruct() override;

	/** Binds to the ship's hub event, and holds the notices if its hub is already open. */
	virtual void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	/** Drops expired notices and repaints while any show. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** One notice on screen. */
	struct FNotice
	{
		/** What it says. */
		FString Text;
		/** Its colour. */
		FLinearColor Color = FLinearColor::White;
		/** HUD time it was added. */
		double Added = 0.0;
	};

	/**
	 * An active quest's title, kept so a bounty that leaves the registry as it completes can still be named. Also the
	 * set of quests seen active: only those get status notices.
	 */
	struct FKnownTitle
	{
		/** The quest. */
		FName QuestId;
		/** Its title. */
		FText Title;
	};

	UFUNCTION()
	void HandleQuestStatusChanged(FName QuestId, EQuestStatus NewStatus);

	UFUNCTION()
	void HandleObjectiveProgress(FName QuestId, int32 ObjectiveIndex, int32 Current, int32 Required);

	UFUNCTION()
	void HandleHubOpenChanged(bool bOpen);

	/** Shows "LL2 empty, switched to LL1" in the Hull colour (UWeaponSystemComponent::OnAmmoAutoSwitched). */
	UFUNCTION()
	void HandleAmmoAutoSwitched(class UAmmoData* Exhausted, class UAmmoData* Replacement);

	/** Shows the tier's first-visit notice. */
	UFUNCTION()
	void HandleFirstVisitToTier(int32 Tier);

	/** Shows "<track> upgraded to level N" for a single track raised (not a load, a reset or a lowered level). */
	UFUNCTION()
	void HandleUpgradeChanged(FName TrackId, int32 NewLevel);

	/**
	 * Hides the notices and holds them, or shows the held ones, newest last, each restarting its lifetime. Holding
	 * stops them expiring unseen.
	 */
	void SetHeld(bool bHold);

	/** Drops the oldest notices beyond the style's limit. */
	void TrimNotices();

	/** Adds a notice, dropping the oldest beyond the style's limit. */
	void AddNotice(const FText& Text, const FLinearColor& Color);

	/** The quest's title, from its definition or the remembered titles. */
	FText FindTitle(FName QuestId) const;

	/** Remembers the title of the quest if it is registered. */
	void RememberTitle(FName QuestId);

	/** The ship bound to, for its hub event. */
	TWeakObjectPtr<AStartechShipPawn> BoundShip;

	/** True while the station hub is open: notices are kept, unshown and not expiring. */
	bool bHeld = false;

	/** The tier subsystem bound to, for first visits. */
	TWeakObjectPtr<class UStartechTierSubsystem> BoundTiers;

	/** The quest subsystem bound to. */
	TWeakObjectPtr<UQuestSubsystem> BoundQuests;

	/** The upgrade subsystem bound to. */
	TWeakObjectPtr<class UShipUpgradeSubsystem> BoundUpgrades;

	/** Each track's last level seen, so only a raise gets a notice. */
	TArray<TPair<FName, int32>> SeenLevels;

	/** Notices on screen, oldest first. */
	TArray<FNotice> Notices;

	/** The quests seen Active or ReadyToTurnIn and not finished since, with their titles. */
	TArray<FKnownTitle> KnownTitles;
};
