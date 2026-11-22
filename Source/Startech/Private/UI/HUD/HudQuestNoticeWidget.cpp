#include "UI/HUD/HudQuestNoticeWidget.h"

#include "AmmoData.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Objectives/QuestObjective.h"
#include "Runtime/QuestSubsystem.h"
#include "StartechShipPawn.h"
#include "Tiers/StartechTierSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "Upgrades/ShipUpgradeSubsystem.h"
#include "WeaponSystemComponent.h"

#define LOCTEXT_NAMESPACE "HudQuestNotice"

namespace QuestNotice
{
	/** Seconds at the end of a notice's life spent fading out. */
	constexpr float FadeTime = 0.6f;
	constexpr float LineGap = 4.f;
}

UHudQuestNoticeWidget::UHudQuestNoticeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 4;
}

void UHudQuestNoticeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UStartechTierSubsystem* const Tiers = IsDesignTime() ? nullptr : UStartechTierSubsystem::Get(this))
	{
		Tiers->OnFirstVisitToTier.AddUniqueDynamic(this, &UHudQuestNoticeWidget::HandleFirstVisitToTier);
		BoundTiers = Tiers;
	}

	if (UShipUpgradeSubsystem* const Upgrades = IsDesignTime() ? nullptr : UShipUpgradeSubsystem::Get(this))
	{
		Upgrades->OnUpgradeChanged.AddUniqueDynamic(this, &UHudQuestNoticeWidget::HandleUpgradeChanged);
		BoundUpgrades = Upgrades;
		HandleUpgradeChanged(NAME_None, 0);
	}

	const UGameInstance* const GameInstance = IsDesignTime() ? nullptr : GetGameInstance();
	UQuestSubsystem* const Quests = GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr;
	BoundQuests = Quests;
	if (!Quests)
	{
		return;
	}

	Quests->OnQuestStatusChanged.AddUniqueDynamic(this, &UHudQuestNoticeWidget::HandleQuestStatusChanged);
	Quests->OnObjectiveProgress.AddUniqueDynamic(this, &UHudQuestNoticeWidget::HandleObjectiveProgress);

	for (const EQuestStatus Status : { EQuestStatus::Active, EQuestStatus::ReadyToTurnIn })
	{
		for (const FName Id : Quests->GetQuestsByStatus(Status))
		{
			RememberTitle(Id);
		}
	}
}

void UHudQuestNoticeWidget::NativeDestruct()
{
	if (UStartechTierSubsystem* const Tiers = BoundTiers.Get())
	{
		Tiers->OnFirstVisitToTier.RemoveDynamic(this, &UHudQuestNoticeWidget::HandleFirstVisitToTier);
	}
	BoundTiers.Reset();

	if (UShipUpgradeSubsystem* const Upgrades = BoundUpgrades.Get())
	{
		Upgrades->OnUpgradeChanged.RemoveDynamic(this, &UHudQuestNoticeWidget::HandleUpgradeChanged);
	}
	BoundUpgrades.Reset();

	if (UQuestSubsystem* const Quests = BoundQuests.Get())
	{
		Quests->OnQuestStatusChanged.RemoveDynamic(this, &UHudQuestNoticeWidget::HandleQuestStatusChanged);
		Quests->OnObjectiveProgress.RemoveDynamic(this, &UHudQuestNoticeWidget::HandleObjectiveProgress);
	}
	BoundQuests.Reset();
	HandlePawnChanged(GetBoundPawn(), nullptr);
	Super::NativeDestruct();
}

void UHudQuestNoticeWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	if (AStartechShipPawn* const OldShip = BoundShip.Get())
	{
		OldShip->OnHubOpenChanged.RemoveDynamic(this, &UHudQuestNoticeWidget::HandleHubOpenChanged);
		if (UWeaponSystemComponent* const Weapons = OldShip->GetWeaponSystem())
		{
			Weapons->OnAmmoAutoSwitched.RemoveDynamic(this, &UHudQuestNoticeWidget::HandleAmmoAutoSwitched);
		}
	}

	Super::HandlePawnChanged(OldPawn, NewPawn);

	AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn);
	BoundShip = Ship;
	if (Ship)
	{
		Ship->OnHubOpenChanged.AddUniqueDynamic(this, &UHudQuestNoticeWidget::HandleHubOpenChanged);
		if (UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem())
		{
			Weapons->OnAmmoAutoSwitched.AddUniqueDynamic(this, &UHudQuestNoticeWidget::HandleAmmoAutoSwitched);
		}
	}
	SetHeld(Ship && Ship->IsHubOpen());
}

void UHudQuestNoticeWidget::HandleAmmoAutoSwitched(UAmmoData* const Exhausted, UAmmoData* const Replacement)
{
	if (Exhausted && Replacement)
	{
		AddNotice(FText::Format(LOCTEXT("AmmoSwitched", "{0} empty, switched to {1}"), Exhausted->DisplayName,
			Replacement->DisplayName), GetHudStyle().HullColor);
	}
}

void UHudQuestNoticeWidget::HandleHubOpenChanged(const bool bOpen)
{
	SetHeld(bOpen);
}

void UHudQuestNoticeWidget::SetHeld(const bool bHold)
{
	if (bHold == bHeld)
	{
		return;
	}
	bHeld = bHold;

	if (bHold)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Everything held shows now, each for its full lifetime.
	const double Now = GetHudTime();
	for (FNotice& Notice : Notices)
	{
		Notice.Added = Now;
	}
	TrimNotices();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RequestRepaint();
}

void UHudQuestNoticeWidget::TrimNotices()
{
	const int32 Excess = Notices.Num() - GetHudStyle().NoticeMaxLines;
	if (Excess > 0)
	{
		Notices.RemoveAt(0, Excess);
	}
}

void UHudQuestNoticeWidget::HandleQuestStatusChanged(const FName QuestId, const EQuestStatus NewStatus)
{
	// Only quests seen active get notices, so a restored save (which re-broadcasts every status) shows none.
	const bool bWasActive = KnownTitles.ContainsByPredicate([QuestId](const FKnownTitle& Each) { return Each.QuestId == QuestId; });
	const FText Title = FindTitle(QuestId);
	const UStartechHudStyle& HudStyle = GetHudStyle();

	switch (NewStatus)
	{
	case EQuestStatus::Active:
		RememberTitle(QuestId);
		break;
	case EQuestStatus::ReadyToTurnIn:
		if (bWasActive)
		{
			AddNotice(FText::Format(LOCTEXT("Ready", "Ready to turn in: {0}"), Title), HudStyle.AdvantageColor);
		}
		RememberTitle(QuestId);
		break;
	case EQuestStatus::Failed:
		if (bWasActive)
		{
			AddNotice(FText::Format(LOCTEXT("Failed", "Quest failed: {0}"), Title), HudStyle.DangerTextColor);
		}
		break;
	case EQuestStatus::Completed:
		if (bWasActive)
		{
			AddNotice(FText::Format(LOCTEXT("Complete", "Quest complete: {0}"), Title), HudStyle.AdvantageColor);
		}
		break;
	default:
		break;
	}

	if (NewStatus != EQuestStatus::Active && NewStatus != EQuestStatus::ReadyToTurnIn)
	{
		KnownTitles.RemoveAll([QuestId](const FKnownTitle& Each) { return Each.QuestId == QuestId; });
	}
}

void UHudQuestNoticeWidget::HandleObjectiveProgress(const FName QuestId, const int32 ObjectiveIndex, const int32 Current,
	const int32 Required)
{
	// Fires on accept with 0 and on every change; only the step that reaches the count is a notice.
	const UQuestSubsystem* const Quests = BoundQuests.Get();
	const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(QuestId) : nullptr;
	const UQuestObjective* const Objective = Definition && Definition->Objectives.IsValidIndex(ObjectiveIndex)
		? Definition->Objectives[ObjectiveIndex].Get() : nullptr;
	if (!Objective || Required <= 0 || Current < Required)
	{
		return;
	}

	AddNotice(FText::Format(LOCTEXT("Objective", "Objective complete: {0}"), Objective->GetProgressText(Current)),
		GetHudStyle().TextColor);
}

void UHudQuestNoticeWidget::HandleFirstVisitToTier(const int32 Tier)
{
	const UStartechTierSubsystem* const Tiers = BoundTiers.Get();
	if (!Tiers)
	{
		return;
	}
	const FSectorTierData& Data = Tiers->GetTierData(Tier);
	if (!Data.FirstVisitNotice.IsEmpty())
	{
		AddNotice(Data.FirstVisitNotice, GetHudStyle().GetTierColor(Data.NoticeColor));
	}
}

void UHudQuestNoticeWidget::HandleUpgradeChanged(const FName TrackId, const int32 NewLevel)
{
	const UShipUpgradeSubsystem* const Upgrades = BoundUpgrades.Get();
	const UUpgradeCatalog* const Catalog = Upgrades ? Upgrades->GetCatalog() : nullptr;
	if (!Catalog)
	{
		return;
	}

	// A load or a new game changes every track at once: remember the levels without notices.
	if (TrackId.IsNone())
	{
		SeenLevels.Reset();
		for (const FUpgradeTrack& Track : Catalog->Tracks)
		{
			SeenLevels.Emplace(Track.TrackId, Upgrades->GetLevel(Track.TrackId));
		}
		return;
	}

	TPair<FName, int32>* Seen = SeenLevels.FindByPredicate([TrackId](const TPair<FName, int32>& Each) { return Each.Key == TrackId; });
	if (!Seen)
	{
		Seen = &SeenLevels.Emplace_GetRef(TrackId, 0);
	}
	const bool bRaised = NewLevel > Seen->Value;
	Seen->Value = NewLevel;

	const FUpgradeTrack* const Track = Catalog->FindTrack(TrackId);
	if (bRaised && Track)
	{
		AddNotice(FText::Format(LOCTEXT("Upgraded", "{0} upgraded to level {1}"), Track->DisplayName, FText::AsNumber(NewLevel)),
			GetHudStyle().PlayerColor);
	}
}

void UHudQuestNoticeWidget::AddNotice(const FText& Text, const FLinearColor& Color)
{
	FNotice& Notice = Notices.AddDefaulted_GetRef();
	Notice.Text = Text.ToString();
	Notice.Color = Color;
	Notice.Added = GetHudTime();

	// While held, every notice is kept and trimmed to the limit when the hub closes.
	if (!bHeld)
	{
		TrimNotices();
		RequestRepaint();
	}
}

FText UHudQuestNoticeWidget::FindTitle(const FName QuestId) const
{
	const UQuestSubsystem* const Quests = BoundQuests.Get();
	if (const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(QuestId) : nullptr)
	{
		return Definition->Title;
	}
	const FKnownTitle* const Known = KnownTitles.FindByPredicate([QuestId](const FKnownTitle& Each)
	{
		return Each.QuestId == QuestId;
	});
	return Known ? Known->Title : FText::FromName(QuestId);
}

void UHudQuestNoticeWidget::RememberTitle(const FName QuestId)
{
	const UQuestSubsystem* const Quests = BoundQuests.Get();
	const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(QuestId) : nullptr;
	if (!Definition)
	{
		return;
	}

	FKnownTitle* const Known = KnownTitles.FindByPredicate([QuestId](const FKnownTitle& Each)
	{
		return Each.QuestId == QuestId;
	});
	if (Known)
	{
		Known->Title = Definition->Title;
	}
	else
	{
		KnownTitles.Add({ QuestId, Definition->Title });
	}
}

void UHudQuestNoticeWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bHeld || Notices.IsEmpty())
	{
		return;
	}

	const double Now = GetHudTime();
	const float Lifetime = GetHudStyle().NoticeLifetime;
	Notices.RemoveAll([Now, Lifetime](const FNotice& Notice) { return Now - Notice.Added >= Lifetime; });
	RequestRepaint();
}

int32 UHudQuestNoticeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 TextLayer = BaseLayer + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float Width = AllottedGeometry.GetLocalSize().X;
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const float LineHeight = FHudDraw::Measure(TEXT("Ag"), Font).Y + QuestNotice::LineGap;
	const double Now = GetHudTime();

	// Newest at the top, just under the tracker.
	float Y = 0.f;
	for (int32 Index = Notices.Num() - 1; Index >= 0; --Index)
	{
		const FNotice& Notice = Notices[Index];
		const float Remaining = HudStyle.NoticeLifetime - static_cast<float>(Now - Notice.Added);
		const float Opacity = FMath::Clamp(Remaining / QuestNotice::FadeTime, 0.f, 1.f);
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Notice.Text, Font, FVector2f(Width, Y),
			FHudDraw::WithOpacity(Notice.Color, Opacity), EHudTextAlign::Right);
		Y += LineHeight;
	}

	return TextLayer;
}

#undef LOCTEXT_NAMESPACE
