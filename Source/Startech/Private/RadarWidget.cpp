#include "RadarWidget.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HudFormat.h"
#include "MapMarkerComponent.h"
#include "MapMarkerSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace RadarWidget
{
	// Horizontal to vertical radius of the ellipse.
	constexpr float EllipseAspect = 2.2f;
	constexpr float LineThickness = 1.f;
	constexpr float PlayerMarkSize = 6.f;
	constexpr float DotSize = 4.f;
	constexpr float BaseTickSize = 2.f;
	constexpr float AccentInset = 10.f;
	constexpr float AccentHeight = 6.f;
	constexpr float LabelHeight = 16.f;
	constexpr float Margin = 4.f;
}

URadarWidget::URadarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 1;
}

void URadarWidget::GatherContacts(const APawn& Pawn, TArray<const UMapMarkerComponent*>& OutContacts) const
{
	const UWorld* const World = GetWorld();
	const UMapMarkerSubsystem* const Markers = World ? World->GetSubsystem<UMapMarkerSubsystem>() : nullptr;
	if (!Markers)
	{
		return;
	}

	for (const TWeakObjectPtr<UMapMarkerComponent>& Entry : Markers->GetMarkers())
	{
		const UMapMarkerComponent* const Marker = Entry.Get();
		// A hidden owner is dead or stored in the pool: still a valid actor, but not in play.
		const AActor* const MarkerOwner = Marker ? Marker->GetOwner() : nullptr;
		if (!Marker || !Marker->IsMarkerEnabled() || !Marker->bShowOnRadar || MarkerOwner == &Pawn
			|| (MarkerOwner && MarkerOwner->IsHidden()))
		{
			continue;
		}
		OutContacts.Add(Marker);
	}
}

int32 URadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 RingLayer = BaseLayer + 1;
	const int32 ContactLayer = BaseLayer + 2;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());

	// A thin accent line along the top with its right end cut at 45 degrees.
	const FLinearColor Accent = FHudDraw::WithOpacity(HudStyle.PlayerColor, 0.6f);
	FHudDraw::Lines(OutDrawElements, RingLayer, AllottedGeometry, {
		FVector2f(0.f, RadarWidget::AccentHeight),
		FVector2f(0.f, 0.f),
		FVector2f(Size.X - RadarWidget::AccentInset, 0.f),
		FVector2f(Size.X, RadarWidget::AccentInset) }, Accent, 1.f);

	// The ellipse fills the space between the accent line and the label.
	const float AreaTop = RadarWidget::AccentHeight + RadarWidget::Margin;
	const float AreaHeight = Size.Y - AreaTop - RadarWidget::LabelHeight - RadarWidget::Margin;
	const FVector2f Centre(Size.X * 0.5f, AreaTop + AreaHeight * 0.5f);
	const float RadiusX = FMath::Min(Size.X * 0.5f - RadarWidget::Margin, AreaHeight * 0.5f * RadarWidget::EllipseAspect);
	const FVector2f Radii(RadiusX, RadiusX / RadarWidget::EllipseAspect);
	if (Radii.Y <= 0.f || Range <= 0.f)
	{
		return BaseLayer;
	}

	const FLinearColor RingColor = FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, 0.6f);
	FHudDraw::Ellipse(OutDrawElements, RingLayer, AllottedGeometry, Centre, Radii, RingColor, RadarWidget::LineThickness);
	FHudDraw::Ellipse(OutDrawElements, RingLayer, AllottedGeometry, Centre, Radii * 0.5f,
		FHudDraw::WithOpacity(RingColor, 0.6f), RadarWidget::LineThickness);
	FHudDraw::Line(OutDrawElements, RingLayer, AllottedGeometry, Centre - FVector2f(Radii.X, 0.f),
		Centre + FVector2f(Radii.X, 0.f), FHudDraw::WithOpacity(RingColor, 0.5f), RadarWidget::LineThickness);
	FHudDraw::Line(OutDrawElements, RingLayer, AllottedGeometry, Centre - FVector2f(0.f, Radii.Y),
		Centre + FVector2f(0.f, Radii.Y), FHudDraw::WithOpacity(RingColor, 0.5f), RadarWidget::LineThickness);

	// "Radar · 1.5 km" under the ellipse.
	const FText Label = FText::Format(NSLOCTEXT("Radar", "Label", "Radar · {0}"), FHudFormat::Distance(Range));
	FHudDraw::Text(OutDrawElements, ContactLayer, AllottedGeometry, Label.ToString(),
		UStartechHudStyle::MakeFont(HudStyle.SmallFontSize), FVector2f(Centre.X, Size.Y - RadarWidget::LabelHeight),
		HudStyle.TextSecondaryColor, EHudTextAlign::Centre);

	// The player: a small triangle pointing up along their heading.
	const float P = RadarWidget::PlayerMarkSize;
	FHudDraw::Lines(OutDrawElements, ContactLayer, AllottedGeometry, {
		Centre + FVector2f(0.f, -P), Centre + FVector2f(P * 0.8f, P * 0.7f), Centre + FVector2f(-P * 0.8f, P * 0.7f),
		Centre + FVector2f(0.f, -P) }, HudStyle.PlayerColor, 1.5f);

	const APawn* const Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return ContactLayer;
	}

	TArray<const UMapMarkerComponent*> Contacts;
	GatherContacts(*Pawn, Contacts);

	const FVector PlayerLocation = Pawn->GetActorLocation();
	const double YawRadians = FMath::DegreesToRadians(static_cast<double>(Pawn->GetActorRotation().Yaw));
	const FVector2D Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
	const FVector2D Right(-Forward.Y, Forward.X);

	// One scale for the plane and the stalks: pixels per cm across the ellipse's long axis.
	const float PixelsPerCm = Radii.X / Range;
	const float Squash = Radii.Y / Radii.X;

	for (const UMapMarkerComponent* const Contact : Contacts)
	{
		const FVector Offset = Contact->GetComponentLocation() - PlayerLocation;
		const FVector2D Flat(Offset.X, Offset.Y);
		const float EffectiveRange = Contact->RadarRange > 0.f ? FMath::Min(Contact->RadarRange, Range) : Range;
		if (Flat.Size() > EffectiveRange)
		{
			continue;
		}

		// Player's right is right; their forward is up the ellipse, squashed by the viewing angle.
		const float LocalRight = static_cast<float>(FVector2D::DotProduct(Flat, Right));
		const float LocalForward = static_cast<float>(FVector2D::DotProduct(Flat, Forward));
		const FVector2f PlanePoint = Centre + FVector2f(LocalRight * PixelsPerCm, -LocalForward * PixelsPerCm * Squash);
		const float StalkHeight = FMath::Clamp(static_cast<float>(Offset.Z) * PixelsPerCm, -PlanePoint.Y,
			Size.Y - PlanePoint.Y);
		const FVector2f DotPoint = PlanePoint - FVector2f(0.f, StalkHeight);

		FLinearColor Color = HudStyle.TextSecondaryColor;
		switch (Contact->GetContactKind())
		{
		case ERadarContactKind::Hostile:
			Color = HudStyle.DangerColor;
			break;
		case ERadarContactKind::Loot:
			Color = HudStyle.CreditsColor;
			break;
		default:
			break;
		}

		// The stalk and its foot on the plane, then the dot at the contact's height.
		if (!FMath::IsNearlyZero(StalkHeight, 0.5f))
		{
			FHudDraw::Line(OutDrawElements, ContactLayer, AllottedGeometry, PlanePoint, DotPoint,
				FHudDraw::WithOpacity(Color, 0.6f), 1.f);
		}
		FHudDraw::Line(OutDrawElements, ContactLayer, AllottedGeometry,
			PlanePoint - FVector2f(RadarWidget::BaseTickSize, 0.f), PlanePoint + FVector2f(RadarWidget::BaseTickSize, 0.f),
			FHudDraw::WithOpacity(Color, 0.6f), 1.f);

		const FVector2f DotSize(RadarWidget::DotSize);
		FHudDraw::Box(OutDrawElements, ContactLayer, AllottedGeometry, DotPoint - DotSize * 0.5f, DotSize, Color);
	}

	return ContactLayer;
}
