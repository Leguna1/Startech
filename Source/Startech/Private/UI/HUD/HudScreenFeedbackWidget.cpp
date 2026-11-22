#include "UI/HUD/HudScreenFeedbackWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Gates/WarpTransitionComponent.h"
#include "HealthComponent.h"
#include "HomeRespawnSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StartechShipPawn.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace ScreenFeedback
{
	const FName ColorParam(TEXT("Color"));
	const FName IntensityParam(TEXT("Intensity"));
	const FName DirectionParam(TEXT("Direction"));
	const FName WidthParam(TEXT("Width"));
	const FName AspectRatioParam(TEXT("AspectRatio"));

	// A hit from within this many cm of the ship counts as having no source: an even glow.
	constexpr double NoSourceDistance = 1.0;

	// The hull flash is shorter than the shield ripple.
	constexpr float HullFlashScale = 0.6f;

	// Fallback edge bars: their thickness as a fraction of the glow width.
	constexpr float FallbackBarScale = 0.35f;

	constexpr float CriticalGap = 18.f;

	// Space between the two lines of the death message.
	constexpr float DeathLineGap = 10.f;

	// Below the reticle's target name and lock label.
	constexpr float WarpRefusalBelowText = 44.f;

	// A refusal not being retried (not from a gate) stays up this long.
	constexpr float WarpRefusalHoldSeconds = 2.f;
	constexpr float MessageHeightFraction = 0.32f;
}

void UHudScreenFeedbackWidget::NativeDestruct()
{
	UnbindAll();
	Super::NativeDestruct();
}

UMaterialInstanceDynamic* UHudScreenFeedbackWidget::MakeEdgeInstance()
{
	UMaterialInterface* const Material = GetHudStyle().EdgeFeedbackMaterial;
	return Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
}

void UHudScreenFeedbackWidget::HandleStyleChanged()
{
	ShieldEdge = MakeEdgeInstance();
	HullEdge = MakeEdgeInstance();
	BoundsEdge = MakeEdgeInstance();

	ShieldBrush = FSlateBrush();
	HullBrush = FSlateBrush();
	BoundsBrush = FSlateBrush();
	ShieldBrush.SetResourceObject(ShieldEdge);
	HullBrush.SetResourceObject(HullEdge);
	BoundsBrush.SetResourceObject(BoundsEdge);
}

void UHudScreenFeedbackWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	UnbindAll();
	Super::HandlePawnChanged(OldPawn, NewPawn);

	AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn);
	UHealthComponent* const Health = Ship ? Ship->GetHealthComponent() : nullptr;
	BoundShip = Ship;
	BoundHealth = Health;

	if (Health)
	{
		Health->OnDamageTaken.AddUniqueDynamic(this, &UHudScreenFeedbackWidget::HandleDamageTaken);
		Health->OnLowHullChanged.AddUniqueDynamic(this, &UHudScreenFeedbackWidget::HandleLowHullChanged);
	}
	if (Ship)
	{
		Ship->OnOutOfBoundsChanged.AddUniqueDynamic(this, &UHudScreenFeedbackWidget::HandleOutOfBoundsChanged);
		Ship->OnCameraModeChanged.AddUniqueDynamic(this, &UHudScreenFeedbackWidget::HandleCameraModeChanged);
	}

	UWarpTransitionComponent* const Warp = NewPawn ? NewPawn->FindComponentByClass<UWarpTransitionComponent>() : nullptr;
	BoundWarp = Warp;
	if (Warp)
	{
		Warp->OnWarpRefused.AddUniqueDynamic(this, &UHudScreenFeedbackWidget::HandleWarpRefused);
		Warp->OnWarpStarted.AddUniqueDynamic(this, &UHudScreenFeedbackWidget::HandleWarpStarted);
	}
}

void UHudScreenFeedbackWidget::RefreshFromPawn()
{
	const UHealthComponent* const Health = BoundHealth.Get();
	const AStartechShipPawn* const Ship = BoundShip.Get();
	bLowHull = Health && Health->IsLowHull();
	bOutOfBounds = Ship && Ship->IsOutsideSector();
	bFirstPerson = Ship && Ship->IsInCockpit();
}

void UHudScreenFeedbackWidget::UnbindAll()
{
	if (UHealthComponent* const Health = BoundHealth.Get())
	{
		Health->OnDamageTaken.RemoveDynamic(this, &UHudScreenFeedbackWidget::HandleDamageTaken);
		Health->OnLowHullChanged.RemoveDynamic(this, &UHudScreenFeedbackWidget::HandleLowHullChanged);
	}
	if (AStartechShipPawn* const Ship = BoundShip.Get())
	{
		Ship->OnOutOfBoundsChanged.RemoveDynamic(this, &UHudScreenFeedbackWidget::HandleOutOfBoundsChanged);
		Ship->OnCameraModeChanged.RemoveDynamic(this, &UHudScreenFeedbackWidget::HandleCameraModeChanged);
	}
	if (UWarpTransitionComponent* const Warp = BoundWarp.Get())
	{
		Warp->OnWarpRefused.RemoveDynamic(this, &UHudScreenFeedbackWidget::HandleWarpRefused);
		Warp->OnWarpStarted.RemoveDynamic(this, &UHudScreenFeedbackWidget::HandleWarpStarted);
	}
	BoundHealth.Reset();
	BoundShip.Reset();
	BoundWarp.Reset();
	WarpRefusalReason = FText::GetEmpty();
}

void UHudScreenFeedbackWidget::HandleWarpRefused(const FText Reason)
{
	WarpRefusalReason = Reason;
	WarpRefusalTime = GetHudTime();
	RequestRepaint();
}

void UHudScreenFeedbackWidget::HandleWarpStarted(const FName DestinationSectorId)
{
	WarpRefusalReason = FText::GetEmpty();
	RequestRepaint();
}

bool UHudScreenFeedbackWidget::IsWarpRefusalShown() const
{
	const UWarpTransitionComponent* const Warp = BoundWarp.Get();
	if (WarpRefusalReason.IsEmpty() || !Warp || Warp->IsWarping())
	{
		return false;
	}
	return Warp->IsRetryingWarp() || GetHudTime() - WarpRefusalTime < ScreenFeedback::WarpRefusalHoldSeconds;
}

void UHudScreenFeedbackWidget::DrawWarpRefusal(FSlateWindowElementList& Out, const int32 Layer,
	const FGeometry& Geometry) const
{
	if (!IsWarpRefusalShown())
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(Geometry.GetLocalSize());
	const AStartechShipPawn* const Ship = BoundShip.Get();
	const float CombatLeft = Ship ? Ship->GetCombatSecondsRemaining() : 0.f;

	const FText Line = CombatLeft > 0.f
		? FText::Format(NSLOCTEXT("HudFeedback", "WarpRefusedCountdown", "{0} · {1} s"), WarpRefusalReason,
			FText::AsNumber(FMath::CeilToInt(CombatLeft)))
		: WarpRefusalReason;
	FHudDraw::Text(Out, Layer, Geometry, Line.ToString(), UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, true),
		FVector2f(Size.X * 0.5f, Size.Y * 0.5f + HudStyle.ReticleTextOffset + ScreenFeedback::WarpRefusalBelowText),
		HudStyle.DangerTextColor, EHudTextAlign::Centre);
}

void UHudScreenFeedbackWidget::HandleDamageTaken(const float Amount, const bool bHitShield, const FVector SourceLocation)
{
	if (bHitShield)
	{
		ShieldHitTime = GetHudTime();
		ShieldHitDirection = GetHitDirection(SourceLocation);
	}
	else
	{
		HullHitTime = GetHudTime();
	}
}

void UHudScreenFeedbackWidget::HandleLowHullChanged(const bool bIsLow)
{
	bLowHull = bIsLow;
}

void UHudScreenFeedbackWidget::HandleOutOfBoundsChanged(const bool bOutside)
{
	// Each spell outside starts at the faint end of the pulse.
	if (bOutside && !bOutOfBounds)
	{
		BoundsPulsePhase = 0.f;
	}
	bOutOfBounds = bOutside;
}

float UHudScreenFeedbackWidget::GetOutOfBoundsPulseRate() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const AStartechShipPawn* const Ship = BoundShip.Get();
	if (!Ship || HudStyle.OutOfBoundsUrgentTime <= 0.f)
	{
		return HudStyle.OutOfBoundsPulseRate;
	}

	// 0 with the urgent time or more left, 1 at the end of the countdown.
	const float Urgency = 1.f - FMath::Clamp(Ship->GetOutOfBoundsSecondsRemaining() / HudStyle.OutOfBoundsUrgentTime, 0.f, 1.f);
	return FMath::Lerp(HudStyle.OutOfBoundsPulseRate, HudStyle.OutOfBoundsPulseRateUrgent, Urgency);
}

void UHudScreenFeedbackWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bOutOfBounds)
	{
		BoundsPulsePhase = FMath::Fmod(BoundsPulsePhase + GetOutOfBoundsPulseRate() * InDeltaTime, 1.f);
	}

	// The death message follows the fade and counts down every frame.
	const UWorld* const World = GetWorld();
	const UHomeRespawnSubsystem* const Respawn = World ? World->GetSubsystem<UHomeRespawnSubsystem>() : nullptr;
	if ((Respawn && Respawn->IsRespawning()) || !WarpRefusalReason.IsEmpty())
	{
		RequestRepaint();
	}
	// Once it is no longer shown (warp started, gate left), drop it so the repaints stop.
	if (!WarpRefusalReason.IsEmpty() && !IsWarpRefusalShown())
	{
		WarpRefusalReason = FText::GetEmpty();
	}
}

float UHudScreenFeedbackWidget::GetScreenCover() const
{
	const APlayerController* const PC = GetOwningPlayer();
	const APlayerCameraManager* const Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	return Camera ? FMath::Clamp(Camera->FadeAmount, 0.f, 1.f) : 1.f;
}

void UHudScreenFeedbackWidget::DrawDeathMessage(FSlateWindowElementList& Out, const int32 Layer,
	const FGeometry& Geometry) const
{
	const UWorld* const World = GetWorld();
	const UHomeRespawnSubsystem* const Respawn = World ? World->GetSubsystem<UHomeRespawnSubsystem>() : nullptr;
	const float Opacity = Respawn && Respawn->IsRespawning() ? GetScreenCover() : 0.f;
	if (Opacity <= 0.f)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(Geometry.GetLocalSize());

	const FText Headline = Respawn->IsReturningHome()
		? FText::Format(NSLOCTEXT("HudFeedback", "DeathReturning", "Ship destroyed. Returning to {0}."),
			Respawn->GetHomeDisplayName())
		: NSLOCTEXT("HudFeedback", "DeathAtHome", "Ship destroyed.");
	const FSlateFontInfo HeadlineFont = UStartechHudStyle::MakeFont(HudStyle.HeadingFontSize, true);

	FText CargoLine;
	if (Respawn->HasDeathCargo())
	{
		const float Remaining = Respawn->GetDeathCargoSecondsRemaining();
		if (Remaining < 0.f)
		{
			CargoLine = FText::Format(NSLOCTEXT("HudFeedback", "DeathCargoKept", "Your cargo remains in {0}."),
				Respawn->GetDeathSectorDisplayName());
		}
		else
		{
			const int32 Seconds = FMath::CeilToInt(Remaining);
			CargoLine = FText::Format(NSLOCTEXT("HudFeedback", "DeathCargo", "Your cargo remains in {0} for {1}"),
				Respawn->GetDeathSectorDisplayName(),
				FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60)));
		}
	}
	const FSlateFontInfo CargoFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, false);

	// Both lines as one block, centred vertically.
	const float HeadlineHeight = FHudDraw::Measure(Headline.ToString(), HeadlineFont).Y;
	const float CargoHeight = CargoLine.IsEmpty() ? 0.f
		: ScreenFeedback::DeathLineGap + FHudDraw::Measure(CargoLine.ToString(), CargoFont).Y;
	const float Top = (Size.Y - HeadlineHeight - CargoHeight) * 0.5f;

	FHudDraw::Text(Out, Layer, Geometry, Headline.ToString(), HeadlineFont, FVector2f(Size.X * 0.5f, Top),
		FHudDraw::WithOpacity(HudStyle.DangerTextColor, Opacity), EHudTextAlign::Centre);
	if (!CargoLine.IsEmpty())
	{
		FHudDraw::Text(Out, Layer, Geometry, CargoLine.ToString(), CargoFont,
			FVector2f(Size.X * 0.5f, Top + HeadlineHeight + ScreenFeedback::DeathLineGap),
			FHudDraw::WithOpacity(HudStyle.TextColor, Opacity), EHudTextAlign::Centre);
	}
}

void UHudScreenFeedbackWidget::HandleCameraModeChanged(const bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
}

FVector2f UHudScreenFeedbackWidget::GetHitDirection(const FVector& SourceLocation) const
{
	const AActor* const Ship = BoundShip.Get();
	const APlayerController* const PC = GetOwningPlayer();
	if (!Ship || !PC || FVector::Dist(SourceLocation, Ship->GetActorLocation()) <= ScreenFeedback::NoSourceDistance)
	{
		return FVector2f::ZeroVector;
	}

	// Relative to the camera's heading, flattened: ahead is the top edge, behind the bottom, sides left and right.
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FRotator Heading(0.f, ViewRotation.Yaw, 0.f);
	const FVector ToSource = SourceLocation - Ship->GetActorLocation();
	const float Ahead = static_cast<float>(FVector::DotProduct(ToSource, Heading.Vector()));
	const float Side = static_cast<float>(FVector::DotProduct(ToSource, FRotationMatrix(Heading).GetUnitAxis(EAxis::Y)));

	const FVector2f Direction(Side, -Ahead);
	return Direction.IsNearlyZero() ? FVector2f::ZeroVector : Direction.GetSafeNormal();
}

void UHudScreenFeedbackWidget::DrawEdge(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	UMaterialInstanceDynamic* const Instance, const FSlateBrush& Brush, const FLinearColor& Color, const float Intensity,
	const FVector2f& Direction) const
{
	if (Intensity <= 0.f)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(Geometry.GetLocalSize());

	if (Instance)
	{
		Instance->SetVectorParameterValue(ScreenFeedback::ColorParam, Color);
		Instance->SetScalarParameterValue(ScreenFeedback::IntensityParam, Intensity);
		Instance->SetVectorParameterValue(ScreenFeedback::DirectionParam, FLinearColor(Direction.X, Direction.Y, 0.f, 0.f));
		Instance->SetScalarParameterValue(ScreenFeedback::WidthParam, HudStyle.EdgeGlowWidth);
		Instance->SetScalarParameterValue(ScreenFeedback::AspectRatioParam, Size.Y > 0.f ? Size.X / Size.Y : 1.f);
		FHudDraw::BrushBox(Out, Layer, Geometry, FVector2f::ZeroVector, Size, Brush, FLinearColor::White);
		return;
	}

	// Without the material: a plain bar on the facing edge, or on all four.
	const float Thickness = FMath::Min(Size.X, Size.Y) * 0.5f * HudStyle.EdgeGlowWidth * ScreenFeedback::FallbackBarScale;
	const FLinearColor BarColor = FHudDraw::WithOpacity(Color, Intensity);
	const bool bAll = Direction.IsNearlyZero();
	const bool bHorizontal = FMath::Abs(Direction.X) > FMath::Abs(Direction.Y);
	if (bAll || (!bHorizontal && Direction.Y < 0.f))
	{
		FHudDraw::Box(Out, Layer, Geometry, FVector2f::ZeroVector, FVector2f(Size.X, Thickness), BarColor);
	}
	if (bAll || (!bHorizontal && Direction.Y > 0.f))
	{
		FHudDraw::Box(Out, Layer, Geometry, FVector2f(0.f, Size.Y - Thickness), FVector2f(Size.X, Thickness), BarColor);
	}
	if (bAll || (bHorizontal && Direction.X < 0.f))
	{
		FHudDraw::Box(Out, Layer, Geometry, FVector2f::ZeroVector, FVector2f(Thickness, Size.Y), BarColor);
	}
	if (bAll || (bHorizontal && Direction.X > 0.f))
	{
		FHudDraw::Box(Out, Layer, Geometry, FVector2f(Size.X - Thickness, 0.f), FVector2f(Thickness, Size.Y), BarColor);
	}
}

int32 UHudScreenFeedbackWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 EdgeLayer = BaseLayer + 1;
	const int32 TextLayer = BaseLayer + 2;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FVector2f Centre = Size * 0.5f;
	const double Now = GetHudTime();
	const float HitTime = FMath::Max(HudStyle.HitEffectTime, 0.01f);

	// Edge effects, each fading over its time.
	if (bOutOfBounds)
	{
		const float Pulse = 0.5f - 0.5f * FMath::Cos(BoundsPulsePhase * UE_TWO_PI);
		DrawEdge(OutDrawElements, EdgeLayer, AllottedGeometry, BoundsEdge, BoundsBrush, HudStyle.DangerColor,
			FMath::Lerp(HudStyle.OutOfBoundsPulseMin, HudStyle.OutOfBoundsPulseMax, Pulse), FVector2f::ZeroVector);
	}
	if (ShieldHitTime >= 0.0)
	{
		const float Intensity = 1.f - FHudDraw::Ease(static_cast<float>(Now - ShieldHitTime) / HitTime);
		DrawEdge(OutDrawElements, EdgeLayer, AllottedGeometry, ShieldEdge, ShieldBrush, HudStyle.ShieldColor, Intensity,
			ShieldHitDirection);
	}
	if (HullHitTime >= 0.0)
	{
		const float Intensity = 1.f - FHudDraw::Ease(static_cast<float>(Now - HullHitTime) / (HitTime * ScreenFeedback::HullFlashScale));
		DrawEdge(OutDrawElements, EdgeLayer, AllottedGeometry, HullEdge, HullBrush, HudStyle.DangerColor, Intensity,
			FVector2f::ZeroVector);
	}

	// While dead only the death message shows over the black screen.
	const UWorld* const World = GetWorld();
	const UHomeRespawnSubsystem* const Respawn = World ? World->GetSubsystem<UHomeRespawnSubsystem>() : nullptr;
	const bool bRespawning = Respawn && Respawn->IsRespawning();

	// Cockpit glass: a thin cut-corner frame inset from the edges.
	if (bFirstPerson && !bRespawning)
	{
		const float Inset = HudStyle.CockpitFrameInset;
		FHudDraw::ChamferOutline(OutDrawElements, EdgeLayer, AllottedGeometry, FVector2f(Inset),
			Size - FVector2f(Inset * 2.f), HudStyle.PanelCornerSize * 3.f,
			FHudDraw::WithOpacity(HudStyle.PlayerColor, HudStyle.CockpitFrameOpacity), 1.5f);
	}

	// "Hull critical", pulsing, just above the reticle's target text.
	if (bLowHull && !bRespawning)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Cos(static_cast<float>(Now) * UE_TWO_PI * HudStyle.LowHullPulseRate);
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, TEXT("Hull critical"),
			UStartechHudStyle::MakeFont(HudStyle.BodyFontSize, true),
			FVector2f(Centre.X, Centre.Y + HudStyle.ReticleTextOffset - ScreenFeedback::CriticalGap),
			FHudDraw::WithOpacity(HudStyle.DangerTextColor, 0.35f + 0.65f * Pulse), EHudTextAlign::Centre);
	}

	// The out-of-bounds message with its countdown.
	if (const AStartechShipPawn* const Ship = BoundShip.Get(); Ship && bOutOfBounds)
	{
		const int32 Seconds = FMath::CeilToInt(Ship->GetOutOfBoundsSecondsRemaining());
		const FText Message = FText::Format(
			NSLOCTEXT("HudFeedback", "OutOfBounds", "Return to the sector. Hull destroyed in {0} s"), FText::AsNumber(Seconds));
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Message.ToString(),
			UStartechHudStyle::MakeFont(HudStyle.HeadingFontSize, true),
			FVector2f(Centre.X, Size.Y * ScreenFeedback::MessageHeightFraction), HudStyle.DangerTextColor, EHudTextAlign::Centre);
	}

	if (!bRespawning)
	{
		DrawWarpRefusal(OutDrawElements, TextLayer, AllottedGeometry);
	}
	DrawDeathMessage(OutDrawElements, TextLayer, AllottedGeometry);

	return TextLayer;
}
