#include "ShopEntryWidget.h"

#include "AmmoData.h"
#include "Blueprint/WidgetTree.h"
#include "Combat/CombatBalance.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "GameFramework/Pawn.h"
#include "LaserWeaponData.h"
#include "ShieldDrainWeaponData.h"
#include "ShopData.h"
#include "ShopTrading.h"
#include "TradeableData.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"

#define LOCTEXT_NAMESPACE "ShopEntryWidget"

namespace ShopEntry
{
	constexpr float Height = 52.f;
	constexpr float Inset = 12.f;
	constexpr float TextTop = 8.f;
	constexpr float LineGap = 4.f;
	constexpr float ChipSize = 10.f;
	constexpr float ChipGap = 8.f;

	/** Each trade button's width, so the columns line up from row to row. */
	constexpr float ButtonWidth = 190.f;
	constexpr float ButtonGap = 8.f;

	/** Where "Held" is right-aligned, measured from the right edge (left of both buttons). */
	constexpr float HeldRight = 2.f * ButtonWidth + ButtonGap + Inset + 24.f;

	constexpr float SeparatorOpacity = 0.2f;
	constexpr float SeparatorHeight = 2.f;
}

UShopEntryWidget::UShopEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UShopEntryWidget::BuildContent()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetHeightOverride(ShopEntry::Height);
	WidgetTree->RootWidget = SizeBox;

	UOverlay* const Overlay = WidgetTree->ConstructWidget<UOverlay>();
	SizeBox->AddChild(Overlay);

	UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* const ButtonsSlot = Overlay->AddChildToOverlay(Buttons);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonsSlot->SetVerticalAlignment(VAlign_Center);
	ButtonsSlot->SetPadding(FMargin(0.f, 0.f, ShopEntry::Inset, 0.f));

	BuyAction = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	BuyAction->SetVariant(EHubButtonVariant::Primary);
	BuyAction->SetFixedWidth(ShopEntry::ButtonWidth);
	BuyAction->SetLabel(LOCTEXT("Buy", "Buy"));
	Buttons->AddChildToHorizontalBox(BuyAction)->SetVerticalAlignment(VAlign_Center);

	SellAction = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	SellAction->SetVariant(EHubButtonVariant::Secondary);
	SellAction->SetFixedWidth(ShopEntry::ButtonWidth);
	SellAction->SetLabel(LOCTEXT("Sell", "Sell"));
	UHorizontalBoxSlot* const SellSlot = Buttons->AddChildToHorizontalBox(SellAction);
	SellSlot->SetVerticalAlignment(VAlign_Center);
	SellSlot->SetPadding(FMargin(ShopEntry::ButtonGap, 0.f, 0.f, 0.f));
}

void UShopEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuyAction->OnClicked.AddUObject(this, &UShopEntryWidget::HandleBuyClicked);
	SellAction->OnClicked.AddUObject(this, &UShopEntryWidget::HandleSellClicked);
}

void UShopEntryWidget::NativeDestruct()
{
	BuyAction->OnClicked.RemoveAll(this);
	SellAction->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void UShopEntryWidget::Setup(UShopData* const InShop, const int32 InEntryIndex)
{
	Shop = InShop;
	EntryIndex = InEntryIndex;
	Refresh();
}

void UShopEntryWidget::Refresh()
{
	const FShopEntry* const Entry = GetEntry();
	const AActor* const Trader = GetTrader();
	if (!Entry || !Entry->Good || !Trader)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	Name = Entry->Good->DisplayName.ToString();
	Detail = GetDetail(*Entry->Good).ToString();
	if (FText LockReason; FShopTrading::IsLocked(*Entry, *Trader, LockReason))
	{
		// The Buy button's label gives the reason ("Reach Glacier (tier III)").
		Detail = FText::Format(LOCTEXT("LockedDetail", "Locked  |  {0}"), FText::FromString(Detail)).ToString();
	}
	Held = FText::Format(LOCTEXT("Held", "Held {0}"), FText::AsNumber(FShopTrading::GetOwnedCount(*Entry, *Trader))).ToString();
	bHasChip = Entry->Good->GetDisplayColor(ChipColor);

	const bool bBuyOffered = FShopTrading::IsBuyOffered(*Entry);
	BuyAction->SetVisibility(bBuyOffered ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	if (bBuyOffered)
	{
		ShowPreview(*BuyAction, Preview(ETradeDirection::Buy, Quantity), Shop->GetPricePlayerPays(*Entry->Good));
	}

	const bool bSellOffered = FShopTrading::IsSellOffered(*Entry);
	SellAction->SetVisibility(bSellOffered ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	if (bSellOffered)
	{
		ShowPreview(*SellAction, Preview(ETradeDirection::Sell, Quantity), Shop->GetPricePlayerReceives(*Entry->Good));
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UShopEntryWidget::SetQuantity(const EShopQuantity InQuantity)
{
	Quantity = InQuantity;
	Refresh();
}

int32 UShopEntryWidget::GetRequestedUnits(const ETradeDirection Direction, const EShopQuantity InQuantity) const
{
	const FShopEntry* const Entry = GetEntry();
	const AActor* const Trader = GetTrader();
	if (!Entry || !Trader)
	{
		return 0;
	}
	if (FShopTrading::IsSingleUnitGood(*Entry))
	{
		return 1;
	}
	if (InQuantity != EShopQuantity::Max)
	{
		return FShopQuantity::GetUnits(InQuantity);
	}

	// Max asks for everything possible; when that is nothing, one unit, so the button can say why.
	const int32 Most = Direction == ETradeDirection::Buy
		? FShopTrading::GetMaxBuyable(*Shop, *Entry, *Trader)
		: FShopTrading::GetMaxSellable(*Entry, *Trader);
	return FMath::Max(Most, 1);
}

FTradeOutcome UShopEntryWidget::Preview(const ETradeDirection Direction, const EShopQuantity InQuantity) const
{
	const FShopEntry* const Entry = GetEntry();
	const AActor* const Trader = GetTrader();
	if (!Entry || !Trader)
	{
		return FTradeOutcome();
	}
	const int32 Units = GetRequestedUnits(Direction, InQuantity);
	return Direction == ETradeDirection::Buy
		? FShopTrading::PreviewBuy(*Shop, *Entry, *Trader, Units)
		: FShopTrading::PreviewSell(*Shop, *Entry, *Trader, Units);
}

void UShopEntryWidget::ShowPreview(UHubButtonWidget& Button, const FTradeOutcome& Outcome, const double UnitPrice) const
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	FNumberFormattingOptions Price;
	Price.MinimumFractionalDigits = 0;
	Price.MaximumFractionalDigits = 2;
	Button.SetSecondLine(FText::Format(LOCTEXT("Each", "{0} cr each"), FText::AsNumber(UnitPrice, &Price)),
		HudStyle.TextSecondaryColor);

	Button.SetButtonEnabled(Outcome.IsSuccess());
	Button.SetLabel(GetTradeLabel(Outcome));
	Button.SetAccent(GetTradeAccent(Outcome), HudStyle.CreditsColor);
}

FText UShopEntryWidget::GetTradeLabel(const FTradeOutcome& Outcome)
{
	const bool bBuy = Outcome.Direction == ETradeDirection::Buy;
	if (!Outcome.IsSuccess())
	{
		// Disabled, the label is the reason, like the Upgrades tab: "Need 12 more credits", "Cargo full".
		return Outcome.Reason.IsEmpty() ? (bBuy ? LOCTEXT("Buy", "Buy") : LOCTEXT("Sell", "Sell")) : Outcome.Reason;
	}

	// The amount actually traded, so a clamped ×100 reads "Buy 34".
	return FText::Format(bBuy ? LOCTEXT("BuyN", "Buy {0}") : LOCTEXT("SellN", "Sell {0}"), FText::AsNumber(Outcome.Quantity));
}

FText UShopEntryWidget::GetTradeAccent(const FTradeOutcome& Outcome)
{
	return Outcome.IsSuccess() ? FText::Format(LOCTEXT("Cr", "{0} cr"), FText::AsNumber(Outcome.Credits)) : FText::GetEmpty();
}

FTradeOutcome UShopEntryWidget::Trade(const ETradeDirection Direction, const bool bForceMax)
{
	const FShopEntry* const Entry = GetEntry();
	AActor* const Trader = GetTrader();
	if (!Entry || !Trader)
	{
		return FTradeOutcome();
	}

	const int32 Units = GetRequestedUnits(Direction, bForceMax ? EShopQuantity::Max : Quantity);
	const FTradeOutcome Outcome = Direction == ETradeDirection::Buy
		? FShopTrading::BuyItem(*Shop, *Entry, *Trader, Units)
		: FShopTrading::SellItem(*Shop, *Entry, *Trader, Units);

	// The screen reports it in the status line; the inventory events refresh the rows.
	OnTraded.Broadcast(Outcome);
	return Outcome;
}

void UShopEntryWidget::HandleBuyClicked()
{
	Trade(ETradeDirection::Buy, BuyAction->GetLastClickModifiers().IsShiftDown());
}

void UShopEntryWidget::HandleSellClicked()
{
	Trade(ETradeDirection::Sell, SellAction->GetLastClickModifiers().IsShiftDown());
}

int32 UShopEntryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo BodyFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const float BodyHeight = FHudDraw::Measure(TEXT("Ag"), BodyFont).Y;

	FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f(ShopEntry::Inset, Size.Y - ShopEntry::SeparatorHeight),
		FVector2f(Size.X - 2.f * ShopEntry::Inset, ShopEntry::SeparatorHeight),
		FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, ShopEntry::SeparatorOpacity));

	// Without a detail line the name sits in the middle of the row.
	const float NameTop = Detail.IsEmpty() ? (Size.Y - BodyHeight) * 0.5f : ShopEntry::TextTop;
	float TextX = ShopEntry::Inset;
	if (bHasChip)
	{
		FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry,
			FVector2f(TextX, NameTop + (BodyHeight - ShopEntry::ChipSize) * 0.5f),
			FVector2f(ShopEntry::ChipSize, ShopEntry::ChipSize), ChipColor);
		TextX += ShopEntry::ChipSize + ShopEntry::ChipGap;
	}

	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Name, BodyFont, FVector2f(TextX, NameTop),
		HudStyle.TextColor, EHudTextAlign::Left);
	if (!Detail.IsEmpty())
	{
		FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Detail, SmallFont,
			FVector2f(TextX, NameTop + BodyHeight + ShopEntry::LineGap), HudStyle.TextSecondaryColor, EHudTextAlign::Left);
	}

	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Held, BodyFont,
		FVector2f(Size.X - ShopEntry::HeldRight, (Size.Y - BodyHeight) * 0.5f), HudStyle.TextSecondaryColor,
		EHudTextAlign::Right);
	return Layer;
}

const UTradeableData* UShopEntryWidget::GetGood() const
{
	const FShopEntry* const Entry = GetEntry();
	return Entry ? Entry->Good.Get() : nullptr;
}

const FShopEntry* UShopEntryWidget::GetEntry() const
{
	return Shop && Shop->Entries.IsValidIndex(EntryIndex) ? &Shop->Entries[EntryIndex] : nullptr;
}

AActor* UShopEntryWidget::GetTrader() const
{
	return GetOwningPlayerPawn();
}

FText UShopEntryWidget::GetDetail(const UTradeableData& Good)
{
	FNumberFormattingOptions OneDecimal;
	OneDecimal.MinimumFractionalDigits = 0;
	OneDecimal.MaximumFractionalDigits = 1;

	// Ranges are stored in cm and shown in m.
	if (const ULaserWeaponData* const Laser = Cast<ULaserWeaponData>(&Good))
	{
		return FText::Format(LOCTEXT("LaserDetail", "Damage {0}  |  {1} shots/s  |  Range {2} m"),
			FText::AsNumber(Laser->Damage, &OneDecimal),
			FText::AsNumber(Laser->FireRate, &OneDecimal),
			FText::AsNumber(Laser->Range / 100.f, &OneDecimal));
	}

	if (const UShieldDrainWeaponData* const Drain = Cast<UShieldDrainWeaponData>(&Good))
	{
		return FText::Format(LOCTEXT("DrainDetail", "Drain {0}/s  |  Range {1} m"),
			FText::AsNumber(Drain->ShieldDrainPerSecond, &OneDecimal),
			FText::AsNumber(Drain->Range / 100.f, &OneDecimal));
	}

	if (const UAmmoData* const Ammo = Cast<UAmmoData>(&Good))
	{
		FNumberFormattingOptions TwoDecimals;
		TwoDecimals.MinimumFractionalDigits = 0;
		TwoDecimals.MaximumFractionalDigits = 2;
		return FText::Format(LOCTEXT("AmmoDetail", "Damage x{0}"), FText::AsNumber(UCombatBalance::GetDamageMultiplier(Ammo), &TwoDecimals));
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
