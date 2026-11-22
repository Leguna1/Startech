#include "MountRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "GameFramework/Pawn.h"
#include "ShipFitting.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "WeaponData.h"
#include "WeaponMountComponent.h"

#define LOCTEXT_NAMESPACE "MountRowWidget"

namespace MountRow
{
	constexpr float Height = 52.f;
	constexpr float Inset = 12.f;
	constexpr float ButtonWidth = 220.f;

	/** Where the fitted weapon's name starts. */
	constexpr float WeaponColumn = 200.f;

	constexpr float SeparatorOpacity = 0.2f;
	constexpr float SeparatorHeight = 2.f;
}

UMountRowWidget::UMountRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UMountRowWidget::BuildContent()
{
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	SizeBox->SetHeightOverride(MountRow::Height);
	WidgetTree->RootWidget = SizeBox;

	UOverlay* const Overlay = WidgetTree->ConstructWidget<UOverlay>();
	SizeBox->AddChild(Overlay);

	FitAction = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	FitAction->SetVariant(EHubButtonVariant::Primary);
	FitAction->SetFixedWidth(MountRow::ButtonWidth);
	UOverlaySlot* const ButtonSlot = Overlay->AddChildToOverlay(FitAction);
	ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(0.f, 0.f, MountRow::Inset, 0.f));
}

void UMountRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FitAction->OnClicked.AddUObject(this, &UMountRowWidget::HandleSwapClicked);
}

void UMountRowWidget::NativeDestruct()
{
	FitAction->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void UMountRowWidget::Setup(UWeaponMountComponent* const InMount)
{
	Mount = InMount;
	Refresh();
}

void UMountRowWidget::Refresh()
{
	const UWeaponMountComponent* const ThisMount = Mount.Get();
	const AActor* const Ship = GetShip();
	if (!ThisMount || !Ship)
	{
		return;
	}

	// A mount without a name in the Blueprint falls back to its component name.
	MountName = (ThisMount->DisplayName.IsEmpty() ? FText::FromString(ThisMount->GetName()) : ThisMount->DisplayName).ToString();

	const UWeaponData* const Fitted = ThisMount->GetFittedWeapon();
	WeaponName = (Fitted ? Fitted->DisplayName : LOCTEXT("Empty", "Empty")).ToString();

	const UWeaponData* const Next = FShipFitting::GetNextFittable(*Ship, *ThisMount);
	FitAction->SetButtonEnabled(Next != nullptr);
	FitAction->SetLabel(Next ? FText::Format(LOCTEXT("FitNext", "Fit {0}"), Next->DisplayName) : LOCTEXT("NoSpare", "No spare"));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UMountRowWidget::HandleSwapClicked()
{
	UWeaponMountComponent* const ThisMount = Mount.Get();
	AActor* const Ship = GetShip();
	if (!ThisMount || !Ship)
	{
		return;
	}

	if (UWeaponData* const Next = FShipFitting::GetNextFittable(*Ship, *ThisMount))
	{
		FShipFitting::FitWeapon(*Ship, *ThisMount, Next);
	}
}

int32 UMountRowWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled) + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo BodyFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const float TextY = (Size.Y - FHudDraw::Measure(TEXT("Ag"), BodyFont).Y) * 0.5f;

	FHudDraw::Box(OutDrawElements, Layer, AllottedGeometry, FVector2f(MountRow::Inset, Size.Y - MountRow::SeparatorHeight),
		FVector2f(Size.X - 2.f * MountRow::Inset, MountRow::SeparatorHeight),
		FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, MountRow::SeparatorOpacity));

	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, MountName, BodyFont, FVector2f(MountRow::Inset, TextY),
		HudStyle.TextSecondaryColor, EHudTextAlign::Left);
	FHudDraw::Text(OutDrawElements, Layer, AllottedGeometry, WeaponName, BodyFont, FVector2f(MountRow::WeaponColumn, TextY),
		HudStyle.TextColor, EHudTextAlign::Left);
	return Layer;
}

AActor* UMountRowWidget::GetShip() const
{
	return GetOwningPlayerPawn();
}

#undef LOCTEXT_NAMESPACE
