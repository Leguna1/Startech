#include "UI/Hub/UpgradeRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "Upgrades/ShipUpgradeSubsystem.h"
#include "Upgrades/UpgradeStatFormat.h"

#define LOCTEXT_NAMESPACE "UpgradeRow"

namespace UpgradeRow
{
	constexpr float Height = 64.f;
	constexpr float Inset = 12.f;
	constexpr float TextTop = 10.f;
	constexpr float LineGap = 5.f;

	/** Room kept on the right for the button or "Max level". */
	constexpr float ButtonReserve = 210.f;

	/** Where the pips and the cost start, as fractions of the width left of ButtonReserve. */
	constexpr float PipsColumn = 0.36f;
	constexpr float CostColumn = 0.68f;

	constexpr float PipWidth = 18.f;
	constexpr float PipHeight = 7.f;
	constexpr float PipGap = 4.f;

	/** Bottom border opacity, as a fraction of the secondary text colour. */
	constexpr float SeparatorOpacity = 0.2f;

	/** Thick enough to survive the UI scale at 720p (a 1-unit line can round away). */
	constexpr float SeparatorHeight = 2.f;
}

UUpgradeRowWidget::UUpgradeRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UUpgradeRowWidget::BuildContent()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetHeightOverride(UpgradeRow::Height);
	WidgetTree->RootWidget = SizeBox;

	UOverlay* const Overlay = WidgetTree->ConstructWidget<UOverlay>();
	SizeBox->AddChild(Overlay);

	Button = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	Button->SetLabel(LOCTEXT("Upgrade", "Upgrade"));
	UOverlaySlot* const ButtonSlot = Overlay->AddChildToOverlay(Button);
	ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(0.f, 0.f, UpgradeRow::Inset, 0.f));
}

void UUpgradeRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Button->OnClicked.AddUObject(this, &UUpgradeRowWidget::HandleUpgradeClicked);
}

void UUpgradeRowWidget::NativeDestruct()
{
	Button->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void UUpgradeRowWidget::Setup(const FName InTrackId)
{
	TrackId = InTrackId;
	Refresh();
}

void UUpgradeRowWidget::Refresh()
{
	const UShipUpgradeSubsystem* const Upgrades = UShipUpgradeSubsystem::Get(this);
	const FUpgradeTrack* const Track = Upgrades ? Upgrades->FindTrack(TrackId) : nullptr;
	const AStartechShipPawn* const Ship = GetShip();
	if (!Track || !Ship)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	Name = Track->DisplayName;
	Description = Track->Description;
	Level = Upgrades->GetLevel(TrackId);
	MaxLevel = Track->GetMaxLevel();
	const bool bMaxed = Level >= MaxLevel;

	// The change at the next level, one part per stat ("Boost 100 → 120 · Boost recharge 20/s → 23/s").
	TArray<FString> Changes;
	for (const FUpgradeStatEffect& Effect : Track->Effects)
	{
		Changes.Add((bMaxed
			? FUpgradeStatFormat::FormatValue(Effect, TrackId, Level, *Ship, *Upgrades)
			: FUpgradeStatFormat::FormatChange(Effect, TrackId, Level, Level + 1, *Ship, *Upgrades)).ToString());
	}
	ChangeText = FString::Join(Changes, TEXT("  ·  "));

	CostParts.Reset();
	FUpgradeLevelCost Cost;
	const UInventoryComponent* const Inventory = Ship->GetInventory();
	if (!bMaxed && Upgrades->GetLevelCost(TrackId, Level + 1, Cost))
	{
		FCostPart& Credits = CostParts.AddDefaulted_GetRef();
		Credits.Text = FText::Format(LOCTEXT("Credits", "{0} credits"), FText::AsNumber(Cost.Credits)).ToString();
		Credits.Color = Inventory->GetCredits() >= Cost.Credits ? HudStyle.CreditsColor : HudStyle.DangerTextColor;

		for (const FUpgradeMineralCost& Mineral : Cost.Minerals)
		{
			const UItemData* const Item = UStartechSettings::Get().FindItemOfType<UItemData>(Mineral.MineralTag);
			const int32 Held = Item ? Inventory->GetItemCount(Item) : 0;
			FCostPart& Part = CostParts.AddDefaulted_GetRef();
			Part.Text = FText::Format(LOCTEXT("Mineral", "{0} {1} ({2})"), FText::AsNumber(Mineral.Count),
				Item ? Item->DisplayName : FText::FromString(Mineral.MineralTag.ToString()), FText::AsNumber(Held)).ToString();
			Part.Color = Held >= Mineral.Count ? HudStyle.AdvantageColor : HudStyle.DangerTextColor;
		}
	}

	FText Reason;
	const bool bCan = Upgrades->CanUpgrade(TrackId, Reason);
	Button->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	Button->SetButtonEnabled(bCan);
	Button->SetLabel(bCan ? LOCTEXT("Upgrade", "Upgrade") : Reason);

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UUpgradeRowWidget::HandleUpgradeClicked()
{
	// The subsystem's OnUpgradeChanged and the inventory events refresh the tab.
	if (UShipUpgradeSubsystem* const Upgrades = UShipUpgradeSubsystem::Get(this))
	{
		Upgrades->Upgrade(TrackId);
	}
}

int32 UUpgradeRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
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
	const float SecondLine = UpgradeRow::TextTop + BodyHeight + UpgradeRow::LineGap;
	const float Columns = FMath::Max(Size.X - UpgradeRow::ButtonReserve, 1.f);

	// A faint line between rows.
	FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f(UpgradeRow::Inset, Size.Y - UpgradeRow::SeparatorHeight),
		FVector2f(Size.X - 2.f * UpgradeRow::Inset, UpgradeRow::SeparatorHeight),
		FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, UpgradeRow::SeparatorOpacity));

	// Name and description.
	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Name.ToString(), BodyFont,
		FVector2f(UpgradeRow::Inset, UpgradeRow::TextTop), HudStyle.TextColor, EHudTextAlign::Left);
	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Description.ToString(), SmallFont,
		FVector2f(UpgradeRow::Inset, SecondLine), HudStyle.TextSecondaryColor, EHudTextAlign::Left);

	// Level pips, then the change at the next level.
	const float PipsX = Columns * UpgradeRow::PipsColumn;
	const float PipsY = UpgradeRow::TextTop + (BodyHeight - UpgradeRow::PipHeight) * 0.5f;
	for (int32 Pip = 0; Pip < MaxLevel; ++Pip)
	{
		FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry,
			FVector2f(PipsX + Pip * (UpgradeRow::PipWidth + UpgradeRow::PipGap), PipsY),
			FVector2f(UpgradeRow::PipWidth, UpgradeRow::PipHeight),
			Pip < Level ? HudStyle.PlayerColor : HudStyle.EmptySegmentColor);
	}
	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, ChangeText, SmallFont, FVector2f(PipsX, SecondLine),
		HudStyle.TextColor, EHudTextAlign::Left);

	// The cost: credits on the first line, minerals on the second.
	const float CostX = Columns * UpgradeRow::CostColumn;
	float MineralX = CostX;
	for (int32 Index = 0; Index < CostParts.Num(); ++Index)
	{
		const FCostPart& Part = CostParts[Index];
		if (Index == 0)
		{
			FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Part.Text, BodyFont,
				FVector2f(CostX, UpgradeRow::TextTop), Part.Color, EHudTextAlign::Left);
			continue;
		}
		FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, Part.Text, SmallFont, FVector2f(MineralX, SecondLine),
			Part.Color, EHudTextAlign::Left);
		MineralX += FHudDraw::Measure(Part.Text + TEXT("   "), SmallFont).X;
	}

	if (MaxLevel > 0 && Level >= MaxLevel)
	{
		const float MaxY = (Size.Y - BodyHeight) * 0.5f;
		FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, LOCTEXT("MaxLevel", "Max level").ToString(), BodyFont,
			FVector2f(Size.X - UpgradeRow::Inset, MaxY), HudStyle.PlayerColor, EHudTextAlign::Right);
	}
	return Layer;
}

#undef LOCTEXT_NAMESPACE
