#include "NavMarkerLayerWidget.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HudFormat.h"
#include "MapMarkerComponent.h"
#include "MapMarkerSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace NavMarkerLayer
{
	constexpr float LineThickness = 1.5f;
	constexpr float LabelGap = 4.f;
}

UNavMarkerLayerWidget::UNavMarkerLayerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 5;
}

int32 UNavMarkerLayerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 ShapeLayer = BaseLayer + 1;
	const int32 TextLayer = BaseLayer + 2;

	const APlayerController* const PC = GetOwningPlayer();
	const APawn* const Pawn = GetOwningPlayerPawn();
	if (!PC || !Pawn)
	{
		return BaseLayer;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FVector PlayerLocation = Pawn->GetActorLocation();
	const FSlateFontInfo LabelFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize, true);
	const FSlateFontInfo DistanceFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const float HalfIcon = HudStyle.MarkerSize * 0.5f;

	TArray<const UMapMarkerComponent*> Markers;
	GatherMarkers(PlayerLocation, Markers);

	for (const UMapMarkerComponent* const Marker : Markers)
	{
		const FVector Location = Marker->GetComponentLocation();
		const float Distance = FVector::Dist(PlayerLocation, Location);
		if (Distance < HudStyle.MarkerHideDistance)
		{
			continue;
		}

		FVector2f Point;
		if (!FHudDraw::ProjectToWidget(*this, *PC, AllottedGeometry, Location, Point) || !FHudDraw::IsInside(Point, Size))
		{
			continue;
		}

		const FLinearColor LabelColor = GetLabelColor(Marker->Type);
		FHudDraw::Diamond(OutDrawElements, ShapeLayer, AllottedGeometry, Point, HalfIcon, LabelColor,
			NavMarkerLayer::LineThickness);

		float Y = Point.Y + HalfIcon + NavMarkerLayer::LabelGap;
		if (!Marker->Label.IsEmpty())
		{
			// "Gate · Glacier · " in the marker colour, then the suffix ("Tier III") in its own colour.
			const FString Suffix = Marker->LabelSuffix.ToString();
			const FString Label = Suffix.IsEmpty() ? Marker->Label.ToString() : Marker->Label.ToString() + TEXT(" · ");
			Y += FHudDraw::TwoColourText(OutDrawElements, TextLayer, AllottedGeometry, Label, LabelColor, Suffix,
				HudStyle.GetTierColor(Marker->LabelSuffixColor), LabelFont, FVector2f(Point.X, Y), EHudTextAlign::Centre).Y;
		}
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, MakeDistanceLine(*Marker, Distance).ToString(),
			DistanceFont, FVector2f(Point.X, Y), HudStyle.TextSecondaryColor, EHudTextAlign::Centre);
	}

	return TextLayer;
}

FLinearColor UNavMarkerLayerWidget::GetLabelColor(const EMapMarkerType Type) const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	switch (Type)
	{
	case EMapMarkerType::Gate:
	case EMapMarkerType::DroppedCargo:
		return HudStyle.CreditsColor;
	case EMapMarkerType::Station:
		return HudStyle.PlayerColor;
	default:
		return HudStyle.TextColor;
	}
}

FText UNavMarkerLayerWidget::MakeDistanceLine(const UMapMarkerComponent& Marker, const float Distance)
{
	const FText DistanceText = FHudFormat::Distance(Distance);
	if (!Marker.HasExpiry())
	{
		return DistanceText;
	}
	return FText::Format(NSLOCTEXT("NavMarkers", "DistanceAndTime", "{0} · {1}"), DistanceText,
		FHudFormat::Countdown(Marker.GetSecondsRemaining()));
}

void UNavMarkerLayerWidget::GatherMarkers(const FVector& PlayerLocation, TArray<const UMapMarkerComponent*>& OutMarkers) const
{
	const UWorld* const World = GetWorld();
	const UMapMarkerSubsystem* const Subsystem = World ? World->GetSubsystem<UMapMarkerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	const APawn* const Pawn = GetOwningPlayerPawn();
	TArray<const UMapMarkerComponent*> Candidates;
	for (const TWeakObjectPtr<UMapMarkerComponent>& Entry : Subsystem->GetMarkers())
	{
		const UMapMarkerComponent* const Marker = Entry.Get();
		// A hidden owner is dead or stored in the pool: still a valid actor, but not in play.
		const AActor* const MarkerOwner = Marker ? Marker->GetOwner() : nullptr;
		if (Marker && Marker->IsMarkerEnabled() && Marker->bShowScreenMarker && MarkerOwner != Pawn
			&& !(MarkerOwner && MarkerOwner->IsHidden()))
		{
			Candidates.Add(Marker);
		}
	}

	for (const UMapMarkerComponent* const Marker : Candidates)
	{
		if (Marker->bNearestOnly)
		{
			// Beaten by any nearer nearest-only marker of its type; ties go to the earlier one.
			const double Distance = FVector::DistSquared(PlayerLocation, Marker->GetComponentLocation());
			const bool bBeaten = Candidates.ContainsByPredicate([Marker, Distance, &PlayerLocation, &Candidates]
				(const UMapMarkerComponent* const Other)
				{
					if (Other == Marker || !Other->bNearestOnly || Other->Type != Marker->Type)
					{
						return false;
					}
					const double OtherDistance = FVector::DistSquared(PlayerLocation, Other->GetComponentLocation());
					return OtherDistance < Distance
						|| (OtherDistance == Distance && Candidates.IndexOfByKey(Other) < Candidates.IndexOfByKey(Marker));
				});

			if (bBeaten)
			{
				continue;
			}
		}

		OutMarkers.Add(Marker);
	}
}
