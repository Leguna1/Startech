#include "UI/HUD/HudActionBarWidget.h"

#include "ActionBar/ActionBarComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "StartechShipPawn.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace HudActionBar
{
	/** Gap between the group labels and the slots. */
	constexpr float LabelGap = 4.f;
	/** Gap between a slot and its underline. */
	constexpr float UnderlineGap = 3.f;
	/** Gap between the underline and the active bar. */
	constexpr float ActiveBarGap = 2.f;
	/** Height of the active bar. */
	constexpr float ActiveBarHeight = 2.f;
	/** Inset of the key label and count from the slot's corners. */
	constexpr float TextInset = 5.f;
	/** Laser icon: bar width, height and gap. */
	constexpr float BarWidth = 4.f;
	constexpr float BarHeight = 18.f;
	/** The icon's centre, as a fraction of the slot from the top: above the middle, clear of the count. */
	constexpr float IconCentreY = 0.42f;
	/** Icon opacity under a cooldown, so the seconds read clearly. */
	constexpr float CooldownIconOpacity = 0.3f;
	constexpr float BarSpacing = 4.f;
	/** Drainer icon: chevron width, height, vertical step and line thickness. */
	constexpr float ChevronWidth = 16.f;
	constexpr float ChevronHeight = 6.f;
	constexpr float ChevronStep = 6.f;
	constexpr float ChevronThickness = 2.f;
	/** Texture icon size. */
	constexpr float TextureSize = 28.f;
	/** Rays in a full cooldown sweep. */
	constexpr int32 SweepRays = 96;
}

UHudActionBarWidget::UHudActionBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 2;
}

void UHudActionBarWidget::HandleStyleChanged()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FLinearColor Base = HudStyle.PanelFillColor;
	NormalPanel.InitializeWithFill(this, HudStyle, FLinearColor(Base.R, Base.G, Base.B, HudStyle.ActionFillOpacity), FLinearColor::Transparent);
	SelectedPanel.InitializeWithFill(this, HudStyle, FLinearColor(Base.R, Base.G, Base.B, HudStyle.ActionSelectedFillOpacity),
		FLinearColor::Transparent);
	ReadSlots(false);
}

void UHudActionBarWidget::NativeDestruct()
{
	UnbindBar();
	Super::NativeDestruct();
}

void UHudActionBarWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	UnbindBar();
	Super::HandlePawnChanged(OldPawn, NewPawn);

	const AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn);
	if (UActionBarComponent* const Bar = Ship ? Ship->GetActionBar() : nullptr)
	{
		BoundBar = Bar;
		SlotsChangedHandle = Bar->OnSlotsChanged.AddUObject(this, &UHudActionBarWidget::HandleSlotsChanged);
	}
}

void UHudActionBarWidget::UnbindBar()
{
	if (UActionBarComponent* const Bar = BoundBar.Get())
	{
		Bar->OnSlotsChanged.Remove(SlotsChangedHandle);
	}
	SlotsChangedHandle.Reset();
	BoundBar.Reset();
}

void UHudActionBarWidget::RefreshFromPawn()
{
	ReadSlots(false);
}

void UHudActionBarWidget::HandleSlotsChanged()
{
	ReadSlots(true);
	RequestRepaint();
}

void UHudActionBarWidget::ReadSlots(const bool bAnimate)
{
	const UActionBarComponent* const Bar = BoundBar.Get();
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const int32 Count = Bar ? Bar->GetSlotCount() : 0;
	const double Now = GetHudTime();

	TArray<FSlotView> Previous = MoveTemp(Slots);
	Slots.Reset(Count);
	Groups.Reset();

	float X = 0.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FActionBarSlot& Data = Bar->GetSlot(Index);
		FSlotView& View = Slots.AddDefaulted_GetRef();
		View.State = Bar->GetSlotState(Index, HudStyle.LowAmmoThreshold);
		View.Visual = Bar->GetSlotVisual(Index);
		View.Key = Bar->GetSlotKeyText(Index).ToString();
		View.GroupLabel = Data.GroupLabel;

		// A new group starts where the label changes.
		const bool bNewGroup = Groups.IsEmpty() || !Groups.Last().Label.EqualTo(Data.GroupLabel);
		if (bNewGroup)
		{
			if (!Groups.IsEmpty())
			{
				X += HudStyle.ActionGroupGap - HudStyle.ActionSlotGap;
			}
			FGroupView& Group = Groups.AddDefaulted_GetRef();
			Group.Label = Data.GroupLabel;
			Group.First = Index;
		}
		Groups.Last().Last = Index;
		View.X = X;
		X += HudStyle.ActionSlotSize + HudStyle.ActionSlotGap;

		if (!Previous.IsValidIndex(Index))
		{
			continue;
		}
		const FSlotView& Old = Previous[Index];
		View.SelectedAt = Old.SelectedAt;
		View.EmptiedAt = Old.EmptiedAt;
		View.ReadyAt = Old.ReadyAt;
		if (!bAnimate)
		{
			continue;
		}
		if (View.State.bSelected && !Old.State.bSelected)
		{
			View.SelectedAt = Now;
		}
		if (View.State.bEmpty && !Old.State.bEmpty)
		{
			View.EmptiedAt = Now;
		}
		if (View.State.CooldownFraction <= 0.f && Old.State.CooldownFraction > 0.f)
		{
			View.ReadyAt = Now;
		}
	}
}

float UHudActionBarWidget::GetContentWidth() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	return Slots.IsEmpty() ? 0.f : Slots.Last().X + HudStyle.ActionSlotSize;
}

float UHudActionBarWidget::GetContentHeight() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float LabelHeight = FHudDraw::Measure(TEXT("Ag"), UStartechHudStyle::MakeFont(HudStyle.ActionTextFontSize)).Y;
	return LabelHeight + HudActionBar::LabelGap + HudStyle.ActionSlotSize + HudActionBar::UnderlineGap
		+ HudStyle.ActionUnderlineHeight + HudActionBar::ActiveBarGap + HudActionBar::ActiveBarHeight;
}

float UHudActionBarWidget::GetSlotCentreY() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float LabelHeight = FHudDraw::Measure(TEXT("Ag"), UStartechHudStyle::MakeFont(HudStyle.ActionTextFontSize)).Y;
	return LabelHeight + HudActionBar::LabelGap + HudStyle.ActionSlotSize * 0.5f;
}

bool UHudActionBarWidget::IsAnimating() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const double Now = GetHudTime();
	for (const FSlotView& View : Slots)
	{
		if (View.State.CooldownFraction > 0.f || View.State.ActiveFraction > 0.f
			|| (View.SelectedAt >= 0.0 && Now - View.SelectedAt < HudStyle.ActionSelectFlashTime)
			|| (View.EmptiedAt >= 0.0 && Now - View.EmptiedAt < HudStyle.ActionEmptyFlashTime)
			|| (View.ReadyAt >= 0.0 && Now - View.ReadyAt < HudStyle.ActionReadyFlashTime))
		{
			return true;
		}
	}
	return false;
}

void UHudActionBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsAnimating())
	{
		return;
	}
	// Cooldowns and active effects change continuously: re-read them while they run.
	bool bTimed = false;
	for (const FSlotView& View : Slots)
	{
		bTimed |= View.State.CooldownFraction > 0.f || View.State.ActiveFraction > 0.f;
	}
	if (bTimed)
	{
		ReadSlots(true);
	}
	RequestRepaint();
}

FString UHudActionBarWidget::FormatCount(const int32 Count)
{
	if (Count < 0)
	{
		return FString(TEXT("∞"));
	}
	if (Count > 9999)
	{
		return FString::Printf(TEXT("%.1fk"), FMath::FloorToFloat(Count / 100.f) / 10.f);
	}
	return FString::FromInt(Count);
}

int32 UHudActionBarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 PanelLayer = BaseLayer + 1;
	const int32 ShapeLayer = BaseLayer + 2;
	const int32 TextLayer = BaseLayer + 3;
	if (Slots.IsEmpty())
	{
		return BaseLayer;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Allotted(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.ActionTextFontSize);
	const float S = HudStyle.ActionSlotSize;
	const float LabelHeight = FHudDraw::Measure(TEXT("Ag"), Font).Y;
	const float OffsetX = FMath::Max((Allotted.X - GetContentWidth()) * 0.5f, 0.f);
	const float SlotY = LabelHeight + HudActionBar::LabelGap;
	const double Now = GetHudTime();

	// Group labels, the active group's in the Player colour.
	for (const FGroupView& Group : Groups)
	{
		bool bActive = false;
		for (int32 Index = Group.First; Index <= Group.Last; ++Index)
		{
			bActive |= Slots[Index].State.bInActiveGroup;
		}
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Group.Label.ToString(), Font,
			FVector2f(OffsetX + Slots[Group.First].X, 0.f), bActive ? HudStyle.PlayerColor : HudStyle.TextSecondaryColor,
			EHudTextAlign::Left);
	}

	for (const FSlotView& View : Slots)
	{
		const FActionSlotState& State = View.State;
		const FVector2f Pos(OffsetX + View.X, SlotY);
		const float Opacity = State.bDisabled ? HudStyle.ActionDisabledOpacity : (State.bEmpty ? HudStyle.ActionEmptyOpacity : 1.f);

		// Panel and border.
		(State.bSelected ? SelectedPanel : NormalPanel).DrawWithOpacity(OutDrawElements, PanelLayer, AllottedGeometry, Pos,
			FVector2f(S, S), HudStyle, Opacity);

		FLinearColor Border = HudStyle.EmptySegmentColor;
		float BorderWidth = 1.f;
		if (State.bSelected)
		{
			BorderWidth = HudStyle.ActionSelectedBorderWidth;
			Border = HudStyle.PlayerColor;
			if (View.SelectedAt >= 0.0 && HudStyle.ActionSelectFlashTime > 0.f)
			{
				const float Flash = 1.f - FMath::Clamp(static_cast<float>(Now - View.SelectedAt) / HudStyle.ActionSelectFlashTime, 0.f, 1.f);
				Border = FMath::Lerp(HudStyle.PlayerColor, FLinearColor::White, 0.6f * Flash);
			}
		}
		FHudDraw::ChamferOutline(OutDrawElements, ShapeLayer, AllottedGeometry, Pos, FVector2f(S, S), HudStyle.PanelCornerSize,
			FHudDraw::WithOpacity(Border, Opacity), BorderWidth);

		// Underline and active bar.
		const float UnderY = Pos.Y + S + HudActionBar::UnderlineGap;
		if (State.bSelected)
		{
			FHudDraw::Box(OutDrawElements, ShapeLayer, AllottedGeometry, FVector2f(Pos.X, UnderY),
				FVector2f(S, HudStyle.ActionUnderlineHeight), FHudDraw::WithOpacity(HudStyle.PlayerColor, Opacity));
		}
		if (State.ActiveFraction > 0.f)
		{
			FHudDraw::Box(OutDrawElements, ShapeLayer, AllottedGeometry,
				FVector2f(Pos.X, UnderY + HudStyle.ActionUnderlineHeight + HudActionBar::ActiveBarGap),
				FVector2f(S * FMath::Clamp(State.ActiveFraction, 0.f, 1.f), HudActionBar::ActiveBarHeight), HudStyle.PlayerColor);
		}

		// Icon.
		const float IconOpacity = Opacity * (State.CooldownFraction > 0.f ? HudActionBar::CooldownIconOpacity : 1.f);
		DrawIcon(OutDrawElements, ShapeLayer, AllottedGeometry, View, Pos, S,
			FHudDraw::WithOpacity(State.bSelected ? HudStyle.PlayerColor : HudStyle.TextColor, IconOpacity));

		// Cooldown sweep, its seconds, and the edge flash when ready.
		if (State.CooldownFraction > 0.f)
		{
			DrawCooldownSweep(OutDrawElements, ShapeLayer, AllottedGeometry, Pos, S, State.CooldownFraction,
				FLinearColor(0.f, 0.f, 0.f, HudStyle.ActionCooldownOpacity));
			const FString Seconds = FString::FromInt(FMath::CeilToInt32(State.CooldownSecondsLeft));
			const FSlateFontInfo SecondsFont = UStartechHudStyle::MakeFont(HudStyle.HeadingFontSize, true);
			const float Height = FHudDraw::Measure(Seconds, SecondsFont).Y;
			FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Seconds, SecondsFont,
				FVector2f(Pos.X + S * 0.5f, Pos.Y + (S - Height) * 0.5f), HudStyle.TextColor, EHudTextAlign::Centre);
		}
		if (View.ReadyAt >= 0.0 && HudStyle.ActionReadyFlashTime > 0.f && Now - View.ReadyAt < HudStyle.ActionReadyFlashTime)
		{
			const float Flash = 1.f - static_cast<float>(Now - View.ReadyAt) / HudStyle.ActionReadyFlashTime;
			FHudDraw::ChamferOutline(OutDrawElements, TextLayer, AllottedGeometry, Pos, FVector2f(S, S), HudStyle.PanelCornerSize,
				FHudDraw::WithOpacity(HudStyle.PlayerColor, Flash), HudStyle.ActionSelectedBorderWidth);
		}

		// Key label, top left: Danger while the target is out of this slot's range.
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, View.Key, Font,
			FVector2f(Pos.X + HudActionBar::TextInset, Pos.Y + HudActionBar::TextInset - 2.f),
			FHudDraw::WithOpacity(State.bOutOfRange ? HudStyle.DangerColor : HudStyle.TextSecondaryColor, Opacity),
			EHudTextAlign::Left);

		// Count, bottom right: amber when low, red 0 when empty (not dimmed), flashing once on reaching 0.
		const FString CountText = FormatCount(State.Count);
		const float CountHeight = FHudDraw::Measure(CountText, Font).Y;
		const FVector2f CountAnchor(Pos.X + S - HudActionBar::TextInset, Pos.Y + S - HudActionBar::TextInset - CountHeight + 2.f);
		FLinearColor CountColor = FHudDraw::WithOpacity(State.bLow ? HudStyle.HullColor : HudStyle.TextColor, Opacity);
		if (State.bEmpty)
		{
			CountColor = HudStyle.DangerColor;
			if (View.EmptiedAt >= 0.0 && HudStyle.ActionEmptyFlashTime > 0.f && Now - View.EmptiedAt < HudStyle.ActionEmptyFlashTime)
			{
				const float Flash = 1.f - static_cast<float>(Now - View.EmptiedAt) / HudStyle.ActionEmptyFlashTime;
				const FVector2f CountSize = FHudDraw::Measure(CountText, Font);
				FHudDraw::Box(OutDrawElements, ShapeLayer, AllottedGeometry, CountAnchor - FVector2f(CountSize.X + 3.f, 1.f),
					CountSize + FVector2f(6.f, 2.f), FHudDraw::WithOpacity(HudStyle.DangerColor, 0.45f * Flash));
				CountColor = FMath::Lerp(HudStyle.DangerColor, FLinearColor::White, 0.5f * Flash);
			}
		}
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, CountText, Font, CountAnchor, CountColor, EHudTextAlign::Right);
	}
	return TextLayer;
}

void UHudActionBarWidget::DrawIcon(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FSlotView& View, const FVector2f& Position, const float Size, const FLinearColor& Color) const
{
	const FVector2f Centre = Position + FVector2f(Size * 0.5f, Size * HudActionBar::IconCentreY);
	const int32 Tier = FMath::Max(View.Visual.Tier, 1);

	switch (View.Visual.Icon)
	{
	case EActionSlotIcon::Bars:
	{
		const float Width = Tier * HudActionBar::BarWidth + (Tier - 1) * HudActionBar::BarSpacing;
		for (int32 Index = 0; Index < Tier; ++Index)
		{
			const float X = Centre.X - Width * 0.5f + Index * (HudActionBar::BarWidth + HudActionBar::BarSpacing);
			FHudDraw::Box(Out, Layer, Geometry, FVector2f(X, Centre.Y - HudActionBar::BarHeight * 0.5f),
				FVector2f(HudActionBar::BarWidth, HudActionBar::BarHeight), Color);
		}
		break;
	}
	case EActionSlotIcon::Chevrons:
	{
		const float Height = HudActionBar::ChevronHeight + (Tier - 1) * HudActionBar::ChevronStep;
		const float HalfWidth = HudActionBar::ChevronWidth * 0.5f;
		for (int32 Index = 0; Index < Tier; ++Index)
		{
			const float Top = Centre.Y - Height * 0.5f + Index * HudActionBar::ChevronStep;
			const TArray<FVector2f> Points = {
				FVector2f(Centre.X - HalfWidth, Top),
				FVector2f(Centre.X, Top + HudActionBar::ChevronHeight),
				FVector2f(Centre.X + HalfWidth, Top) };
			FHudDraw::Lines(Out, Layer, Geometry, Points, Color, HudActionBar::ChevronThickness);
		}
		break;
	}
	case EActionSlotIcon::Texture:
		if (View.Visual.Texture)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(View.Visual.Texture);
			Brush.ImageSize = FVector2D(HudActionBar::TextureSize);
			FHudDraw::BrushBox(Out, Layer, Geometry, Centre - FVector2f(HudActionBar::TextureSize * 0.5f),
				FVector2f(HudActionBar::TextureSize), Brush, Color);
		}
		break;
	case EActionSlotIcon::None:
		break;
	}
}

void UHudActionBarWidget::DrawCooldownSweep(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const float Size, const float Fraction, const FLinearColor& Color)
{
	// A fan of rays from the centre to the square's edge, clockwise from twelve o'clock, over Fraction of a turn.
	const FVector2f Centre = Position + FVector2f(Size * 0.5f);
	const float Half = Size * 0.5f;
	const int32 Rays = FMath::Max(FMath::CeilToInt32(HudActionBar::SweepRays * FMath::Clamp(Fraction, 0.f, 1.f)), 1);
	const float Thickness = 2.f * PI * Half / HudActionBar::SweepRays + 1.f;
	for (int32 Ray = 0; Ray < Rays; ++Ray)
	{
		const float Angle = (static_cast<float>(Ray) + 0.5f) / HudActionBar::SweepRays * 2.f * PI;
		const FVector2f Direction(FMath::Sin(Angle), -FMath::Cos(Angle));
		const float Reach = Half / FMath::Max(FMath::Abs(Direction.X), FMath::Abs(Direction.Y));
		FHudDraw::Line(Out, Layer, Geometry, Centre, Centre + Direction * Reach, Color, Thickness);
	}
}
