#include "UI/HUD/HudPlayerCardWidget.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "HealthComponent.h"
#include "PlayerProfileSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace PlayerCard
{
	constexpr float Padding = 12.f;
	constexpr float PortraitSize = 40.f;
	constexpr float PortraitCorner = 6.f;
	constexpr float BarHeight = 8.f;
	constexpr float BarGap = 3.f;
	constexpr float BarLabelWidth = 52.f;
	constexpr float AccentHeight = 2.f;
	constexpr float AccentLength = 0.35f;
}

UHudPlayerCardWidget::UHudPlayerCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 0;
}

void UHudPlayerCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(this))
	{
		Profile->OnCallsignChanged.AddUniqueDynamic(this, &UHudPlayerCardWidget::HandleCallsignChanged);
	}

	if (Portrait)
	{
		PortraitBrush.SetResourceObject(Portrait);
		PortraitBrush.ImageSize = FVector2D(PlayerCard::PortraitSize);
	}
}

void UHudPlayerCardWidget::NativeDestruct()
{
	if (UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(this))
	{
		Profile->OnCallsignChanged.RemoveDynamic(this, &UHudPlayerCardWidget::HandleCallsignChanged);
	}
	UnbindHealth();

	Super::NativeDestruct();
}

void UHudPlayerCardWidget::HandleStyleChanged()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	Panel.Initialize(this, HudStyle, FHudDraw::WithOpacity(HudStyle.PlayerColor, 0.5f));
}

void UHudPlayerCardWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	Super::HandlePawnChanged(OldPawn, NewPawn);
	UnbindHealth();

	UHealthComponent* const Health = NewPawn ? NewPawn->FindComponentByClass<UHealthComponent>() : nullptr;
	BoundHealth = Health;
	if (!Health)
	{
		return;
	}

	Health->OnShieldChanged.AddUniqueDynamic(this, &UHudPlayerCardWidget::HandleShieldChanged);
	Health->OnHullChanged.AddUniqueDynamic(this, &UHudPlayerCardWidget::HandleHullChanged);
	Health->OnLowHullChanged.AddUniqueDynamic(this, &UHudPlayerCardWidget::HandleLowHullChanged);
}

void UHudPlayerCardWidget::RefreshFromPawn()
{
	RefreshIdentity();

	// Before the pawn's BeginPlay the component still reads zero; its BeginPlay broadcast then fills the bars.
	const UHealthComponent* const Health = BoundHealth.Get();
	SetGaugeImmediately(ShieldGauge, Health ? Health->GetShield() : 0.f, Health ? Health->GetMaxShield() : 0.f);
	SetGaugeImmediately(HullGauge, Health ? Health->GetHull() : 0.f, Health ? Health->GetMaxHull() : 0.f);
	bLowHull = Health && Health->IsLowHull();
}

void UHudPlayerCardWidget::UnbindHealth()
{
	if (UHealthComponent* const Health = BoundHealth.Get())
	{
		Health->OnShieldChanged.RemoveDynamic(this, &UHudPlayerCardWidget::HandleShieldChanged);
		Health->OnHullChanged.RemoveDynamic(this, &UHudPlayerCardWidget::HandleHullChanged);
		Health->OnLowHullChanged.RemoveDynamic(this, &UHudPlayerCardWidget::HandleLowHullChanged);
	}
	BoundHealth.Reset();
}

void UHudPlayerCardWidget::HandleCallsignChanged(const FText NewCallsign)
{
	RefreshIdentity();
	RequestRepaint();
}

void UHudPlayerCardWidget::RefreshIdentity()
{
	const UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(this);
	Callsign = Profile ? Profile->GetCallsign() : NSLOCTEXT("HudPlayerCard", "NoCallsign", "Captain");
	ShipId = Profile ? Profile->GetShipId() : FText::GetEmpty();
}

void UHudPlayerCardWidget::HandleShieldChanged(const float Current, const float Max)
{
	const int32 Before = ShieldGauge.Filled;
	UpdateGauge(ShieldGauge, Current, Max);
	if (ShieldGauge.Filled != Before)
	{
		RequestRepaint();
	}
}

void UHudPlayerCardWidget::HandleHullChanged(const float Current, const float Max)
{
	const int32 Before = HullGauge.Filled;
	UpdateGauge(HullGauge, Current, Max);
	if (HullGauge.Filled != Before)
	{
		RequestRepaint();
	}
}

void UHudPlayerCardWidget::HandleLowHullChanged(const bool bIsLow)
{
	bLowHull = bIsLow;
	RequestRepaint();
}

int32 UHudPlayerCardWidget::GetFilledSegments(const float Current, const float Max, const int32 Segments)
{
	const float Fraction = Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;

	// Any remaining sliver keeps its segment lit, so the bar is empty only at zero.
	return FMath::Clamp(FMath::CeilToInt(Fraction * Segments - KINDA_SMALL_NUMBER), 0, Segments);
}

void UHudPlayerCardWidget::SetGaugeImmediately(FGauge& Gauge, const float Current, const float Max) const
{
	const int32 Segments = FMath::Max(GetHudStyle().HealthSegments, 1);
	Gauge.FlickerUntil.Init(0.0, Segments);
	Gauge.Filled = GetFilledSegments(Current, Max, Segments);
}

void UHudPlayerCardWidget::UpdateGauge(FGauge& Gauge, const float Current, const float Max) const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const int32 Segments = Gauge.FlickerUntil.Num();
	const int32 NewFilled = GetFilledSegments(Current, Max, Segments);

	const double Now = GetHudTime();
	for (int32 Index = NewFilled; Index < Gauge.Filled; ++Index)
	{
		Gauge.FlickerUntil[Index] = Now + HudStyle.SegmentFlickerTime;
	}
	for (int32 Index = Gauge.Filled; Index < NewFilled; ++Index)
	{
		Gauge.FlickerUntil[Index] = 0.0;
	}
	Gauge.Filled = NewFilled;
}

bool UHudPlayerCardWidget::IsFlickering(const FGauge& Gauge, const double Now)
{
	return Gauge.FlickerUntil.ContainsByPredicate([Now](const double Until) { return Until > Now; });
}

void UHudPlayerCardWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const double Now = GetHudTime();
	if (bLowHull || IsFlickering(ShieldGauge, Now) || IsFlickering(HullGauge, Now))
	{
		RequestRepaint();
	}
}

void UHudPlayerCardWidget::DrawGauge(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const FVector2f& Size, const FGauge& Gauge, const FLinearColor& FillColor,
	const bool bPulse) const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const double Now = GetHudTime();
	const float Pulse = bPulse
		? 0.55f + 0.45f * FMath::Cos(static_cast<float>(Now) * UE_TWO_PI * HudStyle.LowHullPulseRate)
		: 1.f;

	FHudDraw::SegmentBar(Out, Layer, Geometry, Position, Size, Gauge.FlickerUntil.Num(), PlayerCard::BarGap,
		[&](const int32 Index)
		{
			if (Index < Gauge.Filled)
			{
				return FHudDraw::WithOpacity(FillColor, Pulse);
			}
			const double Until = Gauge.FlickerUntil[Index];
			const bool bFlickerOn = Until > Now
				&& FMath::FloorToInt(static_cast<float>(Until - Now) * HudStyle.SegmentFlickerRate) % 2 == 0;
			return bFlickerOn ? FillColor : HudStyle.EmptySegmentColor;
		});
}

int32 UHudPlayerCardWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 PanelLayer = BaseLayer + 1;
	const int32 ContentLayer = BaseLayer + 2;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const float P = PlayerCard::Padding;

	// Panel, with a short accent line under its left part.
	const FVector2f PanelSize(Size.X, Size.Y - PlayerCard::AccentHeight - 2.f);
	Panel.Draw(OutDrawElements, PanelLayer, AllottedGeometry, FVector2f::ZeroVector, PanelSize, HudStyle);
	FHudDraw::Box(OutDrawElements, PanelLayer, AllottedGeometry, FVector2f(0.f, Size.Y - PlayerCard::AccentHeight),
		FVector2f(Size.X * PlayerCard::AccentLength, PlayerCard::AccentHeight), HudStyle.PlayerColor);

	// Portrait in a cut-corner frame.
	const FVector2f PortraitPos(P, P);
	const FVector2f PortraitSize(PlayerCard::PortraitSize);
	if (Portrait)
	{
		FHudDraw::BrushBox(OutDrawElements, ContentLayer, AllottedGeometry, PortraitPos, PortraitSize, PortraitBrush,
			FLinearColor::White);
	}
	FHudDraw::ChamferOutline(OutDrawElements, ContentLayer, AllottedGeometry, PortraitPos, PortraitSize,
		PlayerCard::PortraitCorner, HudStyle.PlayerColor, 1.f);

	// Callsign, and the ship id tag right-aligned.
	const float TextX = P + PlayerCard::PortraitSize + P;
	const FSlateFontInfo BodyFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, true);
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, Callsign.ToString(), BodyFont, FVector2f(TextX, P),
		HudStyle.TextColor, EHudTextAlign::Left);
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, ShipId.ToString(), SmallFont,
		FVector2f(Size.X - P, P + 2.f), HudStyle.PlayerColor, EHudTextAlign::Right);

	// Shield and hull bars with their labels, below the callsign.
	const float BarX = TextX + PlayerCard::BarLabelWidth;
	const float BarWidth = Size.X - P - BarX;
	const float ShieldY = P + 46.f;
	const float HullY = ShieldY + 22.f;
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, TEXT("Shield"), SmallFont,
		FVector2f(TextX, ShieldY - 3.f), HudStyle.TextSecondaryColor, EHudTextAlign::Left);
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, TEXT("Hull"), SmallFont,
		FVector2f(TextX, HullY - 3.f), HudStyle.TextSecondaryColor, EHudTextAlign::Left);
	DrawGauge(OutDrawElements, ContentLayer, AllottedGeometry, FVector2f(BarX, ShieldY),
		FVector2f(BarWidth, PlayerCard::BarHeight), ShieldGauge, HudStyle.ShieldColor, false);
	DrawGauge(OutDrawElements, ContentLayer, AllottedGeometry, FVector2f(BarX, HullY),
		FVector2f(BarWidth, PlayerCard::BarHeight), HullGauge, HudStyle.HullColor, bLowHull);

	return ContentLayer;
}
