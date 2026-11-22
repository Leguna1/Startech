#include "UI/HUD/HudSpeedReadoutWidget.h"

#include "Boost/ShipBoostComponent.h"
#include "GameFramework/Pawn.h"
#include "StartechShipBase.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace SpeedReadout
{
	constexpr float CentimetresPerMetre = 100.f;
	constexpr float Padding = 14.f;

	/** The speed text's top edge. */
	constexpr float TextTop = 10.f;

	/** The boost gauge's size and the gap between its segments. */
	constexpr float GaugeWidth = 80.f;
	constexpr float GaugeHeight = 5.f;
	constexpr float GaugeGap = 2.f;

	/** Space between the speed text and the gauge. */
	constexpr float GaugeSpacing = 6.f;
}

UHudSpeedReadoutWidget::UHudSpeedReadoutWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 2;
}

void UHudSpeedReadoutWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	UnbindBoost();
	Super::HandlePawnChanged(OldPawn, NewPawn);

	UShipBoostComponent* const Boost = NewPawn ? NewPawn->FindComponentByClass<UShipBoostComponent>() : nullptr;
	BoundBoost = Boost;
	if (!Boost)
	{
		return;
	}

	Boost->OnBoostEnergyChanged.AddUniqueDynamic(this, &UHudSpeedReadoutWidget::HandleBoostEnergyChanged);
	Boost->OnBoostDenied.AddUniqueDynamic(this, &UHudSpeedReadoutWidget::HandleBoostDenied);
}

void UHudSpeedReadoutWidget::RefreshFromPawn()
{
	const UShipBoostComponent* const Boost = BoundBoost.Get();
	Energy = Boost ? Boost->GetEnergy() : 0.f;
	MaxEnergy = Boost ? Boost->GetMaxEnergy() : 0.f;
	RestartThreshold = Boost ? Boost->GetRestartThreshold() : 0.f;
	DeniedTime = -1.0;
	RequestRepaint();
}

void UHudSpeedReadoutWidget::NativeDestruct()
{
	UnbindBoost();
	Super::NativeDestruct();
}

void UHudSpeedReadoutWidget::UnbindBoost()
{
	if (UShipBoostComponent* const Boost = BoundBoost.Get())
	{
		Boost->OnBoostEnergyChanged.RemoveDynamic(this, &UHudSpeedReadoutWidget::HandleBoostEnergyChanged);
		Boost->OnBoostDenied.RemoveDynamic(this, &UHudSpeedReadoutWidget::HandleBoostDenied);
	}
	BoundBoost.Reset();
}

void UHudSpeedReadoutWidget::HandleBoostEnergyChanged(const float Current, const float Max)
{
	Energy = Current;
	MaxEnergy = Max;

	// The threshold follows the maximum, which the Booster upgrade changes.
	if (const UShipBoostComponent* const Boost = BoundBoost.Get())
	{
		RestartThreshold = Boost->GetRestartThreshold();
	}
	RequestRepaint();
}

void UHudSpeedReadoutWidget::HandleBoostDenied()
{
	DeniedTime = GetHudTime();
	RequestRepaint();
}

int32 UHudSpeedReadoutWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 TextLayer = BaseLayer + 1;

	const AStartechShipBase* const Ship = Cast<AStartechShipBase>(GetOwningPlayerPawn());
	if (!Ship)
	{
		return BaseLayer;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const FText Speed = FText::Format(NSLOCTEXT("HudSpeed", "Speed", "{0} m/s"),
		FText::AsNumber(FMath::RoundToInt(Ship->GetSpeed() / SpeedReadout::CentimetresPerMetre)));
	const float TextHeight = FHudDraw::Measure(Speed.ToString(), Font).Y;
	const float Right = Size.X - SpeedReadout::Padding;

	FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Speed.ToString(), Font,
		FVector2f(Right, SpeedReadout::TextTop), HudStyle.TextSecondaryColor, EHudTextAlign::Right);

	if (MaxEnergy <= 0.f)
	{
		return TextLayer;
	}

	// The gauge, right-aligned under the number.
	const int32 Segments = FMath::Max(HudStyle.BoostSegments, 1);
	const float Fraction = FMath::Clamp(Energy / MaxEnergy, 0.f, 1.f);
	const int32 Filled = FMath::Clamp(FMath::CeilToInt(Fraction * Segments - KINDA_SMALL_NUMBER), 0, Segments);

	const double SinceDenied = DeniedTime >= 0.0 ? GetHudTime() - DeniedTime : -1.0;
	const bool bFlashing = SinceDenied >= 0.0 && SinceDenied < HudStyle.BoostDeniedFlashTime;
	const FLinearColor FillColor = bFlashing ? HudStyle.DangerColor
		: Energy < RestartThreshold ? HudStyle.HullColor
		: HudStyle.PlayerColor;

	// The empty segments flash too, so a refusal on an empty pool is still seen.
	const FLinearColor EmptyColor = bFlashing
		? FHudDraw::WithOpacity(HudStyle.DangerColor, 0.45f)
		: HudStyle.EmptySegmentColor;

	const FVector2f GaugePosition(Right - SpeedReadout::GaugeWidth,
		SpeedReadout::TextTop + TextHeight + SpeedReadout::GaugeSpacing);
	FHudDraw::SegmentBar(OutDrawElements, TextLayer, AllottedGeometry, GaugePosition,
		FVector2f(SpeedReadout::GaugeWidth, SpeedReadout::GaugeHeight), Segments, SpeedReadout::GaugeGap,
		[&](const int32 Index) { return Index < Filled ? FillColor : EmptyColor; });
	return TextLayer;
}
