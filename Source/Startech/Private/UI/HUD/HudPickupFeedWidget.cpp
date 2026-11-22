#include "UI/HUD/HudPickupFeedWidget.h"

#include "AmmoData.h"
#include "InventoryComponent.h"
#include "StartechShipPawn.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace PickupFeed
{
	// Seconds at the end of a line's life spent fading out.
	constexpr float FadeTime = 0.5f;
	constexpr float LineGap = 2.f;
}

UHudPickupFeedWidget::UHudPickupFeedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 3;
}

void UHudPickupFeedWidget::NativeDestruct()
{
	UnbindAll();
	Super::NativeDestruct();
}

void UHudPickupFeedWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	UnbindAll();
	Super::HandlePawnChanged(OldPawn, NewPawn);
	Lines.Reset();

	AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn);
	UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr;
	BoundShip = Ship;
	BoundInventory = Inventory;

	if (Inventory)
	{
		Inventory->OnItemAdded.AddUniqueDynamic(this, &UHudPickupFeedWidget::HandleItemAdded);
		Inventory->OnCreditsChanged.AddUniqueDynamic(this, &UHudPickupFeedWidget::HandleCreditsChanged);
	}
	if (Ship)
	{
		Ship->OnLootCollected.AddUniqueDynamic(this, &UHudPickupFeedWidget::HandleLootCollected);
	}
}

void UHudPickupFeedWidget::UnbindAll()
{
	if (UInventoryComponent* const Inventory = BoundInventory.Get())
	{
		Inventory->OnItemAdded.RemoveDynamic(this, &UHudPickupFeedWidget::HandleItemAdded);
		Inventory->OnCreditsChanged.RemoveDynamic(this, &UHudPickupFeedWidget::HandleCreditsChanged);
	}
	if (AStartechShipPawn* const Ship = BoundShip.Get())
	{
		Ship->OnLootCollected.RemoveDynamic(this, &UHudPickupFeedWidget::HandleLootCollected);
	}
	BoundInventory.Reset();
	BoundShip.Reset();
}

void UHudPickupFeedWidget::HandleItemAdded(const FText DisplayName, const int32 Quantity)
{
	// Sentence case: "+3 iron ore".
	AddLine(FString::Printf(TEXT("+%d %s"), Quantity, *DisplayName.ToString().ToLower()), GetHudStyle().AdvantageColor);
}

void UHudPickupFeedWidget::HandleCreditsChanged(const int32 NewTotal, const int32 Delta)
{
	if (Delta > 0)
	{
		const FText Text = FText::Format(NSLOCTEXT("HudPickupFeed", "Credits", "+{0} credits"), FText::AsNumber(Delta));
		AddLine(Text.ToString(), GetHudStyle().CreditsColor);
	}
}

void UHudPickupFeedWidget::HandleLootCollected(const FLootBundle& Taken)
{
	// Items and credits already arrived through the inventory; only ammo is reported here.
	for (const FAmmoStack& Stack : Taken.Ammo)
	{
		if (Stack.Ammo && Stack.Count >= 1.f)
		{
			AddLine(FString::Printf(TEXT("+%d %s"), FMath::FloorToInt(Stack.Count), *Stack.Ammo->DisplayName.ToString().ToLower()),
				GetHudStyle().AdvantageColor);
		}
	}
}

void UHudPickupFeedWidget::AddLine(const FString& Text, const FLinearColor& Color)
{
	FFeedLine& Line = Lines.AddDefaulted_GetRef();
	Line.Text = Text;
	Line.Color = Color;
	Line.Added = GetHudTime();

	const int32 Excess = Lines.Num() - GetHudStyle().PickupMaxLines;
	if (Excess > 0)
	{
		Lines.RemoveAt(0, Excess);
	}
	RequestRepaint();
}

void UHudPickupFeedWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Lines.IsEmpty())
	{
		return;
	}

	const double Now = GetHudTime();
	const float Lifetime = GetHudStyle().PickupLineLifetime;
	Lines.RemoveAll([Now, Lifetime](const FFeedLine& Line) { return Now - Line.Added >= Lifetime; });
	RequestRepaint();
}

int32 UHudPickupFeedWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 TextLayer = BaseLayer + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const float LineHeight = FHudDraw::Measure(TEXT("+0"), Font).Y + PickupFeed::LineGap;
	const double Now = GetHudTime();

	// Newest at the bottom, stacking upward.
	float Y = Size.Y - LineHeight;
	for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
	{
		const FFeedLine& Line = Lines[Index];
		const float Remaining = HudStyle.PickupLineLifetime - static_cast<float>(Now - Line.Added);
		const float Opacity = FMath::Clamp(Remaining / PickupFeed::FadeTime, 0.f, 1.f);
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Line.Text, Font, FVector2f(Size.X, Y),
			FHudDraw::WithOpacity(Line.Color, Opacity), EHudTextAlign::Right);
		Y -= LineHeight;
	}

	return TextLayer;
}
