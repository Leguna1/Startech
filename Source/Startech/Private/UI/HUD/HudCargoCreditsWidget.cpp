#include "UI/HUD/HudCargoCreditsWidget.h"

#include "GameFramework/Pawn.h"
#include "InventoryComponent.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace CargoCredits
{
	constexpr float Padding = 14.f;
	constexpr float BarY = 34.f;
	constexpr float BarHeight = 8.f;
	constexpr float BarGap = 3.f;
	constexpr float CreditsY = 58.f;
}

UHudCargoCreditsWidget::UHudCargoCreditsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 3;
}

void UHudCargoCreditsWidget::NativeDestruct()
{
	UnbindInventory();
	Super::NativeDestruct();
}

void UHudCargoCreditsWidget::HandleStyleChanged()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	Panel.Initialize(this, HudStyle, FHudDraw::WithOpacity(HudStyle.CreditsColor, 0.4f));
}

void UHudCargoCreditsWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	UnbindInventory();
	Super::HandlePawnChanged(OldPawn, NewPawn);

	UInventoryComponent* const Inventory = NewPawn ? NewPawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	BoundInventory = Inventory;
	if (!Inventory)
	{
		return;
	}

	Inventory->OnCargoChanged.AddUniqueDynamic(this, &UHudCargoCreditsWidget::HandleCargoChanged);
	Inventory->OnCreditsChanged.AddUniqueDynamic(this, &UHudCargoCreditsWidget::HandleCreditsChanged);
}

void UHudCargoCreditsWidget::RefreshFromPawn()
{
	const UInventoryComponent* const Inventory = BoundInventory.Get();
	CargoUsed = Inventory ? Inventory->GetCargoUsed() : 0;
	CargoCapacity = Inventory ? Inventory->GetCargoCapacity() : 0;

	// The total at once: no count-up and no tint.
	CountFrom = Inventory ? Inventory->GetCredits() : 0;
	CountTo = CountFrom;
	CountStart = -1.0;
	LastDeltaSign = 0;
}

void UHudCargoCreditsWidget::UnbindInventory()
{
	if (UInventoryComponent* const Inventory = BoundInventory.Get())
	{
		Inventory->OnCargoChanged.RemoveDynamic(this, &UHudCargoCreditsWidget::HandleCargoChanged);
		Inventory->OnCreditsChanged.RemoveDynamic(this, &UHudCargoCreditsWidget::HandleCreditsChanged);
	}
	BoundInventory.Reset();
}

void UHudCargoCreditsWidget::HandleCargoChanged(const int32 Used, const int32 Capacity)
{
	CargoUsed = Used;
	CargoCapacity = Capacity;
	RequestRepaint();
}

void UHudCargoCreditsWidget::HandleCreditsChanged(const int32 NewTotal, const int32 Delta)
{
	// A change mid-count continues from the number on screen, so it never jumps.
	CountFrom = GetDisplayedCredits();
	CountTo = NewTotal;
	CountStart = GetHudTime();
	LastDeltaSign = Delta > 0 ? 1 : (Delta < 0 ? -1 : 0);
	RequestRepaint();
}

int64 UHudCargoCreditsWidget::GetDisplayedCredits() const
{
	if (CountStart < 0.0)
	{
		return CountTo;
	}
	const float Alpha = static_cast<float>(GetHudTime() - CountStart) / FMath::Max(GetHudStyle().CreditsCountTime, 0.01f);
	if (Alpha >= 1.f)
	{
		return CountTo;
	}
	return CountFrom + static_cast<int64>(FMath::RoundToDouble(static_cast<double>(CountTo - CountFrom) * FHudDraw::Ease(Alpha)));
}

bool UHudCargoCreditsWidget::IsAnimating() const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float Longest = FMath::Max(HudStyle.CreditsCountTime, HudStyle.CreditsTintTime);
	return CountStart >= 0.0 && GetHudTime() - CountStart < Longest;
}

void UHudCargoCreditsWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (CountStart >= 0.0)
	{
		RequestRepaint();
		if (!IsAnimating())
		{
			// One last repaint at the final value and colour, then rest.
			CountStart = -1.0;
		}
	}
}

int32 UHudCargoCreditsWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 PanelLayer = BaseLayer + 1;
	const int32 ContentLayer = BaseLayer + 2;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const float P = CargoCredits::Padding;
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const FSlateFontInfo BodyFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);

	Panel.Draw(OutDrawElements, PanelLayer, AllottedGeometry, FVector2f::ZeroVector, Size, HudStyle);

	// Cargo: label, used / capacity, and the bar.
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, TEXT("Cargo"), BodyFont, FVector2f(P, P),
		HudStyle.TextColor, EHudTextAlign::Left);
	const FText CargoText = FText::Format(NSLOCTEXT("HudCargo", "Used", "{0} / {1}"), FText::AsNumber(CargoUsed),
		FText::AsNumber(CargoCapacity));
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, CargoText.ToString(), BodyFont, FVector2f(Size.X - P, P),
		HudStyle.TextColor, EHudTextAlign::Right);

	const float Fraction = CargoCapacity > 0 ? FMath::Clamp(static_cast<float>(CargoUsed) / CargoCapacity, 0.f, 1.f) : 0.f;
	const int32 Segments = HudStyle.CargoSegments;
	const int32 Filled = FMath::Clamp(FMath::CeilToInt(Fraction * Segments - KINDA_SMALL_NUMBER), 0, Segments);
	const FLinearColor BarColor = Fraction >= 1.f ? HudStyle.DangerColor
		: (Fraction >= HudStyle.CargoWarnFraction ? HudStyle.HullColor : HudStyle.AdvantageColor);
	FHudDraw::SegmentBar(OutDrawElements, ContentLayer, AllottedGeometry, FVector2f(P, CargoCredits::BarY),
		FVector2f(Size.X - P * 2.f, CargoCredits::BarHeight), Segments, CargoCredits::BarGap,
		[&](const int32 Index) { return Index < Filled ? BarColor : HudStyle.EmptySegmentColor; });

	// Credits, counting toward the new total, tinted for a moment after a change.
	FLinearColor CreditsColor = HudStyle.CreditsColor;
	if (CountStart >= 0.0 && LastDeltaSign != 0)
	{
		const float Tint = 1.f - static_cast<float>(GetHudTime() - CountStart) / FMath::Max(HudStyle.CreditsTintTime, 0.01f);
		if (Tint > 0.f)
		{
			const FLinearColor TintColor = LastDeltaSign > 0 ? HudStyle.AdvantageColor : HudStyle.DangerTextColor;
			CreditsColor = FMath::Lerp(HudStyle.CreditsColor, TintColor, FHudDraw::Ease(Tint));
		}
	}

	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, TEXT("Credits"), SmallFont,
		FVector2f(P, CargoCredits::CreditsY + 6.f), HudStyle.TextSecondaryColor, EHudTextAlign::Left);
	const FText CreditsText = FText::Format(NSLOCTEXT("HudCargo", "Credits", "{0} cr"), FText::AsNumber(GetDisplayedCredits()));
	FHudDraw::Text(OutDrawElements, ContentLayer, AllottedGeometry, CreditsText.ToString(),
		UStartechHudStyle::MakeFont(HudStyle.CreditsFontSize, true), FVector2f(Size.X - P, CargoCredits::CreditsY),
		CreditsColor, EHudTextAlign::Right);

	return ContentLayer;
}
