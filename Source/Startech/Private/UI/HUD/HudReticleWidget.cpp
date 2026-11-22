#include "UI/HUD/HudReticleWidget.h"

#include "EnemyData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HealthComponent.h"
#include "HudFormat.h"
#include "Kismet/GameplayStatics.h"
#include "MapMarkerComponent.h"
#include "StartechEnemyShip.h"
#include "TargetableComponent.h"
#include "TargetingComponent.h"
#include "TargetRangeComponent.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace Reticle
{
	// Each arc spans this many degrees, centred on the left (shield) or right (hull).
	constexpr float ArcSpan = 100.f;
	// Screen angles (90 is down). The shield arc starts bottom left and steps clockwise up the left side; the hull arc
	// starts bottom right and steps anticlockwise up the right side, so both fill upward.
	constexpr float ShieldStart = 180.f - ArcSpan * 0.5f;
	constexpr float HullStart = ArcSpan * 0.5f;
	constexpr float SegmentGapDegrees = 3.f;
	constexpr float TickLength = 4.f;
	constexpr float SoftThicknessScale = 0.6f;
	constexpr float SoftOpacity = 0.7f;
	constexpr float CentreCircleRadius = 4.f;
	constexpr float CentreTickInner = 8.f;
	constexpr float CentreTickOuter = 14.f;
	constexpr float BracketArm = 8.f;
	// Hard-lock brackets: line width, arm length as a fraction of the frame's width and height, clamped.
	constexpr float HardBracketThickness = 2.5f;
	constexpr float HardArmFraction = 0.4f;
	constexpr float HardArmMin = 10.f;
	constexpr float HardArmMax = 24.f;
	// Hard-lock brackets start this much larger on engage and grow to the release scale as they fade.
	constexpr float EngageScale = 1.6f;
	constexpr float ReleaseScale = 1.4f;
	// Lock ring: gap outside the brackets' corners, dot size, and fade-in after the bracket snap.
	constexpr float RingGap = 6.f;
	constexpr float RingDotSize = 2.5f;
	constexpr float RingFadeTime = 0.2f;

	/** Min..Max scaled by Scale about its centre. */
	void ScaleFrame(const FVector2f& Min, const FVector2f& Max, const float Scale, FVector2f& OutMin, FVector2f& OutMax)
	{
		const FVector2f FrameCentre = (Min + Max) * 0.5f;
		const FVector2f Half = (Max - Min) * 0.5f * Scale;
		OutMin = FrameCentre - Half;
		OutMax = FrameCentre + Half;
	}

	/** Solid hard-lock brackets around Min..Max, arms sized from the frame as drawn. */
	void DrawHardBrackets(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
		const FVector2f& Min, const FVector2f& Max, const FLinearColor& Color)
	{
		const FVector2f Size = Max - Min;
		const FVector2f Arm(FMath::Clamp(Size.X * HardArmFraction, HardArmMin, HardArmMax),
			FMath::Clamp(Size.Y * HardArmFraction, HardArmMin, HardArmMax));
		FHudDraw::CornerBrackets(Out, Layer, Geometry, Min, Max, Arm, Color, HardBracketThickness);
	}
	constexpr float PercentGap = 8.f;
	constexpr float LineGap = 2.f;
}

UHudReticleWidget::UHudReticleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 5;
}

void UHudReticleWidget::NativeDestruct()
{
	UnbindAll();
	Super::NativeDestruct();
}

void UHudReticleWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	UnbindAll();
	Super::HandlePawnChanged(OldPawn, NewPawn);

	UTargetingComponent* const Targeting = NewPawn ? NewPawn->FindComponentByClass<UTargetingComponent>() : nullptr;
	UTargetRangeComponent* const Range = NewPawn ? NewPawn->FindComponentByClass<UTargetRangeComponent>() : nullptr;
	BoundTargeting = Targeting;
	BoundRange = Range;

	if (Range)
	{
		Range->OnRangeStateChanged.AddUniqueDynamic(this, &UHudReticleWidget::HandleRangeStateChanged);
	}
	if (Targeting)
	{
		Targeting->OnTargetChanged.AddUniqueDynamic(this, &UHudReticleWidget::HandleTargetChanged);
	}
}

void UHudReticleWidget::RefreshFromPawn()
{
	const UTargetingComponent* const Targeting = BoundTargeting.Get();
	UTargetableComponent* const NewTarget = Targeting ? Targeting->GetCurrentTarget() : nullptr;
	const ETargetLockState NewState = Targeting && NewTarget ? Targeting->GetLockState() : ETargetLockState::None;

	// Shown as already settled: no bracket slide, hard-lock close-in, release or lock tone.
	HardLockStart = -1.0;
	ReleaseStart = -1.0;
	SlideStart = -1.0;
	bHasLastFrame = false;
	bHasReleaseFrame = false;
	ReleaseRingAlpha = 0.f;

	Target = NewTarget;
	LockState = NewState;
	TargetName = GetTargetName(NewTarget ? NewTarget->GetOwner() : nullptr);
	BindTargetHealth(NewTarget);

	const UTargetRangeComponent* const Range = BoundRange.Get();
	RangeState = Range ? Range->GetRangeState() : ETargetRangeState::OutOfRange;
}

void UHudReticleWidget::UnbindAll()
{
	if (UTargetingComponent* const Targeting = BoundTargeting.Get())
	{
		Targeting->OnTargetChanged.RemoveDynamic(this, &UHudReticleWidget::HandleTargetChanged);
	}
	if (UTargetRangeComponent* const Range = BoundRange.Get())
	{
		Range->OnRangeStateChanged.RemoveDynamic(this, &UHudReticleWidget::HandleRangeStateChanged);
	}
	BindTargetHealth(nullptr);
	BoundTargeting.Reset();
	BoundRange.Reset();
	Target.Reset();
}

void UHudReticleWidget::HandleTargetChanged(UTargetableComponent* const NewTarget, const ETargetLockState NewState)
{
	const ETargetLockState PreviousState = LockState;
	const UTargetableComponent* const PreviousTarget = Target.Get();
	const double Now = GetHudTime();
	const UStartechHudStyle& HudStyle = GetHudStyle();

	// Hard-lock engage and release come from the state transitions.
	if (PreviousState != ETargetLockState::Hard && NewState == ETargetLockState::Hard)
	{
		HardLockStart = Now;
		ReleaseStart = -1.0;
		if (HudStyle.LockTone)
		{
			UGameplayStatics::PlaySound2D(this, HudStyle.LockTone);
		}
	}
	else if (PreviousState == ETargetLockState::Hard && NewState != ETargetLockState::Hard)
	{
		ReleaseStart = Now;
		HardLockStart = -1.0;
	}

	// A soft lock moving straight to another target slides its brackets across.
	const bool bSoftSwitch = NewState == ETargetLockState::Soft && PreviousState == ETargetLockState::Soft
		&& NewTarget && NewTarget != PreviousTarget && bHasLastFrame;
	if (bSoftSwitch)
	{
		SlideStart = Now;
		SlideFromMin = LastFrameMin;
		SlideFromMax = LastFrameMax;
	}
	else if (NewState != ETargetLockState::Soft)
	{
		SlideStart = -1.0;
	}

	if (!NewTarget || NewState == ETargetLockState::None)
	{
		bHasLastFrame = false;
	}

	Target = NewTarget;
	LockState = NewTarget ? NewState : ETargetLockState::None;
	TargetName = GetTargetName(NewTarget ? NewTarget->GetOwner() : nullptr);
	BindTargetHealth(NewTarget);

	if (const UTargetRangeComponent* const Range = BoundRange.Get())
	{
		RangeState = Range->GetRangeState();
	}
}

void UHudReticleWidget::BindTargetHealth(const UTargetableComponent* const NewTarget)
{
	if (UHealthComponent* const Old = TargetHealth.Get())
	{
		Old->OnShieldChanged.RemoveDynamic(this, &UHudReticleWidget::HandleTargetShieldChanged);
		Old->OnHullChanged.RemoveDynamic(this, &UHudReticleWidget::HandleTargetHullChanged);
	}

	const AActor* const Owner = NewTarget ? NewTarget->GetOwner() : nullptr;
	UHealthComponent* const Health = Owner ? Owner->FindComponentByClass<UHealthComponent>() : nullptr;
	TargetHealth = Health;
	if (!Health)
	{
		ShieldFraction = 0.f;
		HullFraction = 0.f;
		return;
	}

	Health->OnShieldChanged.AddUniqueDynamic(this, &UHudReticleWidget::HandleTargetShieldChanged);
	Health->OnHullChanged.AddUniqueDynamic(this, &UHudReticleWidget::HandleTargetHullChanged);
	HandleTargetShieldChanged(Health->GetShield(), Health->GetMaxShield());
	HandleTargetHullChanged(Health->GetHull(), Health->GetMaxHull());
}

void UHudReticleWidget::HandleRangeStateChanged(const ETargetRangeState NewState)
{
	RangeState = NewState;
}

void UHudReticleWidget::HandleTargetShieldChanged(const float Current, const float Max)
{
	ShieldFraction = Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;
}

void UHudReticleWidget::HandleTargetHullChanged(const float Current, const float Max)
{
	HullFraction = Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;
}

FText UHudReticleWidget::GetTargetName(const AActor* const TargetActor)
{
	if (const AStartechEnemyShip* const Enemy = Cast<AStartechEnemyShip>(TargetActor))
	{
		if (const UEnemyData* const Data = Enemy->GetEnemyData(); Data && !Data->DisplayName.IsEmpty())
		{
			return Data->DisplayName;
		}
	}
	if (const UMapMarkerComponent* const Marker = TargetActor ? TargetActor->FindComponentByClass<UMapMarkerComponent>() : nullptr;
		Marker && !Marker->Label.IsEmpty())
	{
		return Marker->Label;
	}
	return NSLOCTEXT("HudReticle", "UnknownTarget", "Contact");
}

FLinearColor UHudReticleWidget::GetRangeColor() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	switch (RangeState)
	{
	case ETargetRangeState::InEnemyRange:
		return HudStyle.DangerTextColor;
	case ETargetRangeState::PlayerAdvantage:
		return HudStyle.AdvantageColor;
	default:
		return HudStyle.TextColor;
	}
}

bool UHudReticleWidget::GetTargetFrame(const FGeometry& Geometry, FVector2f& OutMin, FVector2f& OutMax) const
{
	const APlayerController* const PC = GetOwningPlayer();
	const UTargetableComponent* const Current = Target.Get();
	const AActor* const Owner = Current ? Current->GetOwner() : nullptr;
	if (!PC || !Owner)
	{
		return false;
	}

	const FVector2f WidgetSize(Geometry.GetLocalSize());
	FVector2f AimPoint;
	if (!FHudDraw::ProjectToWidget(*this, *PC, Geometry, Current->GetAimLocation(), AimPoint)
		|| !FHudDraw::IsInside(AimPoint, WidgetSize))
	{
		return false;
	}

	OutMin = AimPoint;
	OutMax = AimPoint;

	FVector Origin;
	FVector Extent;
	Owner->GetActorBounds(true, Origin, Extent);
	if (!Extent.IsNearlyZero())
	{
		bool bAllProjected = true;
		FVector2f Min(TNumericLimits<float>::Max());
		FVector2f Max(-TNumericLimits<float>::Max());
		for (int32 Corner = 0; Corner < 8 && bAllProjected; ++Corner)
		{
			const FVector Offset((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y,
				(Corner & 4) ? Extent.Z : -Extent.Z);
			FVector2f Point;
			bAllProjected = FHudDraw::ProjectToWidget(*this, *PC, Geometry, Origin + Offset, Point);
			Min = FVector2f::Min(Min, Point);
			Max = FVector2f::Max(Max, Point);
		}

		// A corner behind the camera would mirror; keep the aim-point frame then.
		if (bAllProjected)
		{
			OutMin = Min;
			OutMax = Max;
		}
	}

	// Never smaller than the minimum frame, grown around its centre.
	const float MinSize = GetHudStyle().MinFrameSize;
	const FVector2f Centre = (OutMin + OutMax) * 0.5f;
	const FVector2f Half = FVector2f::Max((OutMax - OutMin) * 0.5f, FVector2f(MinSize * 0.5f));
	OutMin = Centre - Half;
	OutMax = Centre + Half;
	return true;
}

void UHudReticleWidget::DrawSegmentedArc(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Centre, const float StartDegrees, const float Step, const int32 Filled, const FLinearColor& FillColor,
	const float Thickness, const float Opacity) const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const int32 Segments = HudStyle.HealthSegments;
	const float HalfGap = Reticle::SegmentGapDegrees * 0.5f * FMath::Sign(Step);

	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float From = StartDegrees + Step * Index + HalfGap;
		const float To = StartDegrees + Step * (Index + 1) - HalfGap;
		const FLinearColor Color = Index < Filled ? FillColor : HudStyle.EmptySegmentColor;
		FHudDraw::Arc(Out, Layer, Geometry, Centre, HudStyle.ArcRadius, From, To, FHudDraw::WithOpacity(Color, Opacity),
			Thickness);
	}

	// The outer tick ring over the same span, ticked at every segment boundary.
	const FLinearColor TickColor = FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, Opacity);
	FHudDraw::Arc(Out, Layer, Geometry, Centre, HudStyle.TickRingRadius, StartDegrees, StartDegrees + Step * Segments,
		FHudDraw::WithOpacity(TickColor, 0.5f), 1.f);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Radians = FMath::DegreesToRadians(StartDegrees + Step * Index);
		const FVector2f Direction(FMath::Cos(Radians), FMath::Sin(Radians));
		FHudDraw::Line(Out, Layer, Geometry, Centre + Direction * HudStyle.TickRingRadius,
			Centre + Direction * (HudStyle.TickRingRadius + Reticle::TickLength), TickColor, 1.f);
	}
}

void UHudReticleWidget::DrawLockRing(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Min, const FVector2f& Max, const float Alpha, const double Now) const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const int32 Dots = FMath::Max(HudStyle.LockRingDotCount, 3);
	const float Opacity = Alpha * HudStyle.LockRingOpacity;
	if (!HudStyle.bShowLockRing || Opacity <= 0.f)
	{
		return;
	}

	const FLinearColor BaseColor = RangeState == ETargetRangeState::InEnemyRange ? HudStyle.DangerColor : HudStyle.PlayerColor;
	const FLinearColor Color = FHudDraw::WithOpacity(BaseColor, Opacity);
	const FVector2f RingCentre = (Min + Max) * 0.5f;
	const float Radius = (Max - Min).Size() * 0.5f + Reticle::RingGap;

	// One turn every LockRingSecondsPerTurn; the remainder keeps the angle small however long the game runs.
	const double Period = FMath::Max(HudStyle.LockRingSecondsPerTurn, 0.1f);
	const float Turn = static_cast<float>(FMath::Fmod(Now, Period) / Period);
	const FVector2f DotSize(Reticle::RingDotSize);
	for (int32 Index = 0; Index < Dots; ++Index)
	{
		const float Radians = (Turn + static_cast<float>(Index) / Dots) * UE_TWO_PI;
		const FVector2f Point = RingCentre + FVector2f(FMath::Cos(Radians), FMath::Sin(Radians)) * Radius;
		FHudDraw::Box(Out, Layer, Geometry, Point - DotSize * 0.5f, DotSize, Color);
	}
}

int32 UHudReticleWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 ShapeLayer = BaseLayer + 1;
	const int32 TextLayer = BaseLayer + 2;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FVector2f Centre = Size * 0.5f;
	const double Now = GetHudTime();
	const float SnapTime = FMath::Max(HudStyle.LockSnapTime, 0.01f);

	const UTargetableComponent* const Current = Target.Get();
	const bool bHasTarget = Current && Current->IsTargetable() && LockState != ETargetLockState::None;
	const bool bHard = bHasTarget && LockState == ETargetLockState::Hard;

	// The frame on the target: thin grey brackets for a soft lock, solid Player-colour ones for a hard lock, both on
	// the projected bounds. Not drawn while off screen.
	FVector2f FrameMin;
	FVector2f FrameMax;
	if (bHasTarget && GetTargetFrame(AllottedGeometry, FrameMin, FrameMax))
	{
		if (bHard)
		{
			const float Snap = HardLockStart >= 0.0 ? FHudDraw::Ease(static_cast<float>(Now - HardLockStart) / SnapTime) : 1.f;
			FVector2f DrawMin;
			FVector2f DrawMax;
			Reticle::ScaleFrame(FrameMin, FrameMax, FMath::Lerp(Reticle::EngageScale, 1.f, Snap), DrawMin, DrawMax);
			Reticle::DrawHardBrackets(OutDrawElements, ShapeLayer, AllottedGeometry, DrawMin, DrawMax, HudStyle.PlayerColor);

			// The ring fades in once the brackets have closed; a lock shown already settled has it at once.
			const float RingAlpha = HardLockStart >= 0.0
				? FMath::Clamp(static_cast<float>(Now - HardLockStart - SnapTime) / Reticle::RingFadeTime, 0.f, 1.f)
				: 1.f;
			DrawLockRing(OutDrawElements, ShapeLayer, AllottedGeometry, DrawMin, DrawMax, RingAlpha, Now);

			ReleaseFrameMin = FrameMin;
			ReleaseFrameMax = FrameMax;
			ReleaseRingAlpha = RingAlpha;
			bHasReleaseFrame = true;
		}
		else
		{
			if (SlideStart >= 0.0)
			{
				const float Slide = FHudDraw::Ease(static_cast<float>(Now - SlideStart) / SnapTime);
				FrameMin = FMath::Lerp(SlideFromMin, FrameMin, Slide);
				FrameMax = FMath::Lerp(SlideFromMax, FrameMax, Slide);
			}
			FHudDraw::CornerBrackets(OutDrawElements, ShapeLayer, AllottedGeometry, FrameMin, FrameMax,
				Reticle::BracketArm, HudStyle.SoftLockColor, 1.f);
		}
		LastFrameMin = FrameMin;
		LastFrameMax = FrameMax;
		bHasLastFrame = true;
	}

	// A released hard lock's brackets expand and fade out where they last were.
	if (!bHard && ReleaseStart >= 0.0 && bHasReleaseFrame)
	{
		const float Release = static_cast<float>(Now - ReleaseStart) / SnapTime;
		if (Release < 1.f)
		{
			FVector2f DrawMin;
			FVector2f DrawMax;
			Reticle::ScaleFrame(ReleaseFrameMin, ReleaseFrameMax,
				FMath::Lerp(1.f, Reticle::ReleaseScale, FHudDraw::Ease(Release)), DrawMin, DrawMax);
			Reticle::DrawHardBrackets(OutDrawElements, ShapeLayer, AllottedGeometry, DrawMin, DrawMax,
				FHudDraw::WithOpacity(HudStyle.PlayerColor, 1.f - Release));
			DrawLockRing(OutDrawElements, ShapeLayer, AllottedGeometry, DrawMin, DrawMax,
				ReleaseRingAlpha * (1.f - Release), Now);
		}
	}

	// The centre mark: a circle with two side ticks; the circle turns Player colour under a hard lock.
	const FLinearColor MarkColor = FHudDraw::WithOpacity(HudStyle.TextColor, 0.8f);
	FHudDraw::Arc(OutDrawElements, ShapeLayer, AllottedGeometry, Centre, Reticle::CentreCircleRadius, 0.f, 360.f,
		bHard ? HudStyle.PlayerColor : MarkColor, 1.f);
	FHudDraw::Line(OutDrawElements, ShapeLayer, AllottedGeometry, Centre - FVector2f(Reticle::CentreTickOuter, 0.f),
		Centre - FVector2f(Reticle::CentreTickInner, 0.f), MarkColor, 1.f);
	FHudDraw::Line(OutDrawElements, ShapeLayer, AllottedGeometry, Centre + FVector2f(Reticle::CentreTickInner, 0.f),
		Centre + FVector2f(Reticle::CentreTickOuter, 0.f), MarkColor, 1.f);

	if (!bHasTarget)
	{
		return TextLayer;
	}

	// The target's shield (left) and hull (right) arcs stay at the reticle, on screen or not.
	const float Thickness = HudStyle.ArcThickness * (bHard ? 1.f : Reticle::SoftThicknessScale);
	const float Opacity = bHard ? 1.f : Reticle::SoftOpacity;
	const int32 Segments = HudStyle.HealthSegments;
	const float Step = Reticle::ArcSpan / Segments;
	const auto FilledOf = [Segments](const float Fraction)
	{
		return FMath::Clamp(FMath::CeilToInt(Fraction * Segments - KINDA_SMALL_NUMBER), 0, Segments);
	};
	DrawSegmentedArc(OutDrawElements, ShapeLayer, AllottedGeometry, Centre, Reticle::ShieldStart, Step,
		FilledOf(ShieldFraction), HudStyle.ShieldColor, Thickness, Opacity);
	DrawSegmentedArc(OutDrawElements, ShapeLayer, AllottedGeometry, Centre, Reticle::HullStart, -Step,
		FilledOf(HullFraction), HudStyle.HullColor, Thickness, Opacity);

	// Percentages beside the arcs.
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const float PercentY = Centre.Y - FHudDraw::Measure(TEXT("0"), SmallFont).Y * 0.5f;
	const float PercentOffset = HudStyle.TickRingRadius + Reticle::TickLength + Reticle::PercentGap;
	FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry,
		FString::Printf(TEXT("%d%%"), FMath::RoundToInt(ShieldFraction * 100.f)), SmallFont,
		FVector2f(Centre.X - PercentOffset, PercentY), FHudDraw::WithOpacity(HudStyle.ShieldColor, Opacity),
		EHudTextAlign::Right);
	FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry,
		FString::Printf(TEXT("%d%%"), FMath::RoundToInt(HullFraction * 100.f)), SmallFont,
		FVector2f(Centre.X + PercentOffset, PercentY), FHudDraw::WithOpacity(HudStyle.HullColor, Opacity),
		EHudTextAlign::Left);

	// Name and distance, coloured by range state, then the lock label.
	const UTargetRangeComponent* const Range = BoundRange.Get();
	const float Distance = Range ? Range->GetDistanceToTarget() : 0.f;
	const FText NameLine = FText::Format(NSLOCTEXT("HudReticle", "NameLine", "{0} · {1}"), TargetName,
		FHudFormat::Distance(Distance));
	const float NameY = Centre.Y + HudStyle.ReticleTextOffset;
	const FVector2f NameSize = FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, NameLine.ToString(),
		UStartechHudStyle::MakeFont(HudStyle.BodyFontSize), FVector2f(Centre.X, NameY), GetRangeColor(), EHudTextAlign::Centre);

	const FString LockLabel = bHard ? TEXT("Hard lock") : TEXT("Soft lock");
	FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, LockLabel, SmallFont,
		FVector2f(Centre.X, NameY + NameSize.Y + Reticle::LineGap), bHard ? HudStyle.PlayerColor : HudStyle.SoftLockColor,
		EHudTextAlign::Centre);

	return TextLayer;
}
