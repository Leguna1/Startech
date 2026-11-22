#include "UI/Hub/HubTabRailWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Runtime/QuestSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

#define LOCTEXT_NAMESPACE "HubTabRail"

namespace HubTabRail
{
	/** Rows: Quests, Shop and Upgrades are available; the rest are not yet. */
	constexpr int32 RowCount = 5;
	constexpr int32 AvailableRows = 3;
	constexpr float TopPadding = 8.f;
	/** Space between the available tabs and the later ones. */
	constexpr float GroupGap = 16.f;
	constexpr float BarWidth = 3.f;
	constexpr float TextInset = 14.f;
	constexpr float BadgePadding = 6.f;

	FText RowLabel(const int32 Row)
	{
		switch (Row)
		{
		case 0: return LOCTEXT("Quests", "Quests");
		case 1: return LOCTEXT("Shop", "Shop");
		case 2: return LOCTEXT("Upgrades", "Upgrades");
		case 3: return LOCTEXT("ShipLog", "Ship log");
		default: return LOCTEXT("Achievements", "Achievements");
		}
	}

	EHubTab RowTab(const int32 Row)
	{
		return Row == 0 ? EHubTab::Quests : Row == 1 ? EHubTab::Shop : EHubTab::Upgrades;
	}
}

UHubTabRailWidget::UHubTabRailWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
}

void UHubTabRailWidget::BuildContent()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetWidthOverride(GetHudStyle().HubTabRailWidth);
	WidgetTree->RootWidget = SizeBox;
}

void UHubTabRailWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->OnQuestStatusChanged.AddUniqueDynamic(this, &UHubTabRailWidget::HandleQuestStatusChanged);
		Quests->OnQuestUnregistered.AddUniqueDynamic(this, &UHubTabRailWidget::HandleQuestUnregistered);
		BoundQuests = Quests;
	}
	RefreshReadyCount();
}

void UHubTabRailWidget::NativeDestruct()
{
	if (UQuestSubsystem* const Quests = BoundQuests.Get())
	{
		Quests->OnQuestStatusChanged.RemoveDynamic(this, &UHubTabRailWidget::HandleQuestStatusChanged);
		Quests->OnQuestUnregistered.RemoveDynamic(this, &UHubTabRailWidget::HandleQuestUnregistered);
	}
	BoundQuests.Reset();
	Super::NativeDestruct();
}

void UHubTabRailWidget::SetSelectedTab(const EHubTab Tab)
{
	Selected = Tab;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHubTabRailWidget::HandleQuestStatusChanged(const FName QuestId, const EQuestStatus NewStatus)
{
	RefreshReadyCount();
}

void UHubTabRailWidget::HandleQuestUnregistered(const FName QuestId)
{
	RefreshReadyCount();
}

void UHubTabRailWidget::RefreshReadyCount()
{
	const UQuestSubsystem* const Quests = BoundQuests.Get();
	ReadyCount = Quests ? Quests->GetQuestsByStatus(EQuestStatus::ReadyToTurnIn).Num() : 0;
	Invalidate(EInvalidateWidgetReason::Paint);
}

float UHubTabRailWidget::GetRowTop(const int32 Index) const
{
	return HubTabRail::TopPadding + Index * GetHudStyle().HubTabHeight
		+ (Index >= HubTabRail::AvailableRows ? HubTabRail::GroupGap : 0.f);
}

int32 UHubTabRailWidget::HitTest(const FVector2f& Position) const
{
	const float Height = GetHudStyle().HubTabHeight;
	for (int32 Row = 0; Row < HubTabRail::RowCount; ++Row)
	{
		const float Top = GetRowTop(Row);
		if (Position.Y >= Top && Position.Y < Top + Height)
		{
			return Row;
		}
	}
	return INDEX_NONE;
}

int32 UHubTabRailWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float Width = AllottedGeometry.GetLocalSize().X;
	const float Height = HudStyle.HubTabHeight;
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);

	for (int32 Row = 0; Row < HubTabRail::RowCount; ++Row)
	{
		const float Top = GetRowTop(Row);
		const bool bAvailable = Row < HubTabRail::AvailableRows;
		const bool bSelected = bAvailable && HubTabRail::RowTab(Row) == Selected;
		const FString Label = HubTabRail::RowLabel(Row).ToString();
		const float TextY = Top + (Height - FHudDraw::Measure(Label, Font).Y) * 0.5f;

		if (bSelected)
		{
			FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f(0.f, Top), FVector2f(Width, Height),
				HudStyle.PanelFillColor);
			FHudDraw::Box(OutDrawElements, Layer + 1, AllottedGeometry, FVector2f(0.f, Top),
				FVector2f(HubTabRail::BarWidth, Height), HudStyle.PlayerColor);
		}
		else if (bAvailable && Row == HoveredRow)
		{
			FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f(0.f, Top), FVector2f(Width, Height),
				FHudDraw::WithOpacity(HudStyle.PanelFillColor, 0.5f));
		}

		const FLinearColor TextColor = !bAvailable ? FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, HudStyle.ButtonDisabledOpacity)
			: bSelected ? HudStyle.TextColor : HudStyle.TextSecondaryColor;
		FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, Label, Font,
			FVector2f(HubTabRail::TextInset, TextY), TextColor, EHudTextAlign::Left);

		// The gold count of quests ready to turn in, right-aligned on the Quests tab.
		if (Row == 0 && ReadyCount > 0)
		{
			const FSlateFontInfo BadgeFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize, true);
			const FString Count = FString::FromInt(ReadyCount);
			const FVector2f CountSize = FHudDraw::Measure(Count, BadgeFont);
			const FVector2f BadgeSize(FMath::Max(CountSize.X + HubTabRail::BadgePadding, CountSize.Y + 2.f), CountSize.Y + 2.f);
			const FVector2f BadgePos(Width - HubTabRail::BadgePadding - BadgeSize.X, Top + (Height - BadgeSize.Y) * 0.5f);
			FHudDraw::Box(OutDrawElements, Layer + 1, AllottedGeometry, BadgePos, BadgeSize, HudStyle.CreditsColor);
			FHudDraw::Text(OutDrawElements, Layer + 2, AllottedGeometry, Count, BadgeFont,
				FVector2f(BadgePos.X + BadgeSize.X * 0.5f, BadgePos.Y + 1.f), HudStyle.EmptySegmentColor, EHudTextAlign::Centre);
		}
	}

	FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, LOCTEXT("ComingLater", "Coming later").ToString(),
		SmallFont, FVector2f(HubTabRail::TextInset, GetRowTop(HubTabRail::RowCount)),
		FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, HudStyle.ButtonDisabledOpacity), EHudTextAlign::Left);

	return Layer + 2;
}

FReply UHubTabRailWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 Row = HitTest(FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())));
	if (Row != HoveredRow)
	{
		HoveredRow = Row;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Unhandled();
}

void UHubTabRailWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	HoveredRow = INDEX_NONE;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply UHubTabRailWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const int32 Row = HitTest(FVector2f(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())));
	if (Row != INDEX_NONE && Row < HubTabRail::AvailableRows && HubTabRail::RowTab(Row) != Selected)
	{
		SetSelectedTab(HubTabRail::RowTab(Row));
		OnTabSelected.Broadcast(Selected);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
