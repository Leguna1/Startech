#include "UI/Hub/QuestListRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Definitions/QuestDefinition.h"
#include "HudFormat.h"
#include "Runtime/QuestSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Quest/QuestUiFormat.h"

#define LOCTEXT_NAMESPACE "QuestListRow"

namespace QuestListRow
{
	constexpr float Inset = 10.f;
	constexpr float TitleTop = 7.f;
	constexpr float LineGap = 3.f;
	constexpr float BorderWidth = 1.5f;
}

UQuestListRowWidget::UQuestListRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
}

void UQuestListRowWidget::BuildContent()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetHeightOverride(GetHudStyle().HubListRowHeight);
	WidgetTree->RootWidget = SizeBox;
}

void UQuestListRowWidget::Setup(const FName InQuestId)
{
	QuestId = InQuestId;
	Refresh();
}

void UQuestListRowWidget::Refresh()
{
	const UQuestSubsystem* const Quests = GetQuests();
	const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(QuestId) : nullptr;
	const FQuestRuntimeState* const State = Quests ? Quests->FindState(QuestId) : nullptr;
	if (!Definition || !State)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	Title = Definition->Title;
	Summary = FQuestUiFormat::Summary(*Definition, *State);
	bCoolingDown = FQuestUiFormat::IsCoolingDown(*State);
	StateText = FText::GetEmpty();
	StateColor = HudStyle.TextSecondaryColor;

	if (State->Status == EQuestStatus::ReadyToTurnIn)
	{
		StateText = LOCTEXT("Ready", "Ready");
		StateColor = HudStyle.AdvantageColor;
	}
	else if (FQuestUiFormat::IsTimerRunning(*Definition, *State))
	{
		StateText = FHudFormat::Countdown(State->TimeRemainingSeconds);
		StateColor = FQuestUiFormat::TimerColor(State->TimeRemainingSeconds, HudStyle);
	}
	else if (bCoolingDown)
	{
		StateText = FText::Format(LOCTEXT("Cooldown", "Available in {0}"), FHudFormat::Countdown(State->CooldownRemainingSeconds));
	}
	else if (State->Status == EQuestStatus::Available && Definition->IsGenerated())
	{
		// A bounty offer: its reward.
		StateText = FText::Format(LOCTEXT("Reward", "{0} credits"), FText::AsNumber(FQuestUiFormat::CreditsReward(*Definition)));
		StateColor = HudStyle.CreditsColor;
	}
	else if (State->Status == EQuestStatus::Completed)
	{
		StateText = LOCTEXT("Done", "Done");
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UQuestListRowWidget::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UQuestListRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());

	if (bSelected || bHovered)
	{
		FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f::ZeroVector, Size,
			FHudDraw::WithOpacity(HudStyle.PanelFillColor, bSelected ? 1.f : 0.5f));
	}
	if (bSelected)
	{
		const FVector2f Max = Size - FVector2f(QuestListRow::BorderWidth * 0.5f);
		const FVector2f Min(QuestListRow::BorderWidth * 0.5f);
		FHudDraw::Lines(OutDrawElements, Layer + 1, AllottedGeometry,
			{ Min, FVector2f(Max.X, Min.Y), Max, FVector2f(Min.X, Max.Y), Min }, HudStyle.PlayerColor, QuestListRow::BorderWidth);
	}

	const FSlateFontInfo TitleFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const float TitleHeight = FHudDraw::Measure(Title.ToString(), TitleFont).Y;

	FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, Title.ToString(), TitleFont,
		FVector2f(QuestListRow::Inset, QuestListRow::TitleTop), HudStyle.TextColor, EHudTextAlign::Left);
	FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, StateText.ToString(), TitleFont,
		FVector2f(Size.X - QuestListRow::Inset, QuestListRow::TitleTop), StateColor, EHudTextAlign::Right);
	FHudDraw::Text(OutDrawElements, Layer + 1, AllottedGeometry, Summary.ToString(), SmallFont,
		FVector2f(QuestListRow::Inset, QuestListRow::TitleTop + TitleHeight + QuestListRow::LineGap),
		HudStyle.TextSecondaryColor, EHudTextAlign::Left);

	return Layer + 1;
}

void UQuestListRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UQuestListRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply UQuestListRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	OnClicked.Broadcast(QuestId);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
