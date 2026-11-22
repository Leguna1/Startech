#include "UI/Hub/QuestObjectiveRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace QuestObjectiveRow
{
	constexpr float BoxSize = 10.f;
	constexpr float TextLeft = 20.f;
	constexpr float BarGap = 3.f;
	constexpr float SegmentGap = 2.f;
}

UQuestObjectiveRowWidget::UQuestObjectiveRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UQuestObjectiveRowWidget::BuildContent()
{
	SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	WidgetTree->RootWidget = SizeBox;
	UpdateHeight();
}

void UQuestObjectiveRowWidget::UpdateHeight()
{
	if (SizeBox)
	{
		const UStartechHudStyle& HudStyle = GetHudStyle();
		SizeBox->SetHeightOverride(HudStyle.HubObjectiveRowHeight
			+ (HasBar() ? HudStyle.HubObjectiveBarHeight + QuestObjectiveRow::BarGap * 2.f : 0.f));
	}
}

void UQuestObjectiveRowWidget::SetObjective(const FText& InText, const int32 InCurrent, const int32 InRequired)
{
	Text = InText;
	Required = FMath::Max(InRequired, 1);
	Current = FMath::Clamp(InCurrent, 0, Required);

	UpdateHeight();
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UQuestObjectiveRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float Width = AllottedGeometry.GetLocalSize().X;
	const float LineHeight = HudStyle.HubObjectiveRowHeight;
	const bool bDone = Current >= Required;
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const float TextY = (LineHeight - FHudDraw::Measure(Text.ToString(), Font).Y) * 0.5f;

	// The checkbox: an outline, filled when done.
	const FVector2f BoxPos(0.f, (LineHeight - QuestObjectiveRow::BoxSize) * 0.5f);
	const FVector2f BoxSize(QuestObjectiveRow::BoxSize);
	if (bDone)
	{
		FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, BoxPos, BoxSize, HudStyle.AdvantageColor);
	}
	else
	{
		const FVector2f Max = BoxPos + BoxSize;
		FHudDraw::Lines(OutDrawElements, Layer, AllottedGeometry,
			{ BoxPos, FVector2f(Max.X, BoxPos.Y), Max, FVector2f(BoxPos.X, Max.Y), BoxPos }, HudStyle.TextSecondaryColor, 1.f);
	}

	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Text.ToString(), Font,
		FVector2f(QuestObjectiveRow::TextLeft, TextY), bDone ? HudStyle.TextSecondaryColor : HudStyle.TextColor,
		EHudTextAlign::Left);
	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, FString::Printf(TEXT("%d / %d"), Current, Required), Font,
		FVector2f(Width, TextY), bDone ? HudStyle.AdvantageColor : HudStyle.TextSecondaryColor, EHudTextAlign::Right);

	if (HasBar())
	{
		const int32 Segments = FMath::Max(HudStyle.HubObjectiveSegments, 1);
		const int32 Filled = FMath::FloorToInt(static_cast<float>(Current) / Required * Segments);
		const FLinearColor FillColor = bDone ? HudStyle.AdvantageColor : HudStyle.PlayerColor;
		FHudDraw::SegmentBar(OutDrawElements, Layer, AllottedGeometry,
			FVector2f(QuestObjectiveRow::TextLeft, LineHeight + QuestObjectiveRow::BarGap),
			FVector2f(Width - QuestObjectiveRow::TextLeft, HudStyle.HubObjectiveBarHeight), Segments,
			QuestObjectiveRow::SegmentGap, [Filled, &FillColor, &HudStyle](const int32 Index)
			{
				return Index < Filled ? FillColor : HudStyle.EmptySegmentColor;
			});
	}

	return Layer;
}
