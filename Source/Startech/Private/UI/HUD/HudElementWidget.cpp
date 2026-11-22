#include "UI/HUD/HudElementWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/Pawn.h"
#include "Misc/App.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

UHudElementWidget::UHudElementWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHudElementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	HandleStyleChanged();
	SyncToPawn();
}

void UHudElementWidget::SetInheritedStyle(const UStartechHudStyle* const InStyle)
{
	if (InheritedStyle.Get() == InStyle)
	{
		return;
	}
	InheritedStyle = InStyle;
	HandleStyleChanged();

	// Segment counts and the like may differ in the new style, so the values are re-applied.
	SyncToPawn();
}

const UStartechHudStyle& UHudElementWidget::GetHudStyle() const
{
	if (Style)
	{
		return *Style;
	}
	if (const UStartechHudStyle* const Inherited = InheritedStyle.Get())
	{
		return *Inherited;
	}
	return UStartechHudStyle::GetDefaultStyle();
}

void UHudElementWidget::BindToPawn(APawn* const OldPawn, APawn* const NewPawn)
{
	HandlePawnChanged(OldPawn, NewPawn);
	SyncToPawn();
}

void UHudElementWidget::SyncToPawn()
{
	RefreshFromPawn();
	RequestRepaint();
}

void UHudElementWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	BoundPawn = NewPawn;
}

double UHudElementWidget::GetHudTime()
{
	return FApp::GetCurrentTime();
}

UWidget* UHudElementWidget::GetLayoutWidget()
{
	UWidget* Layout = this;
	while (UPanelWidget* const Parent = Layout->GetParent())
	{
		if (Parent->IsA<UCanvasPanel>())
		{
			break;
		}
		Layout = Parent;
	}
	return Layout;
}

void UHudElementWidget::SetFoldAmount(const float Folded)
{
	const float Clamped = FMath::Clamp(Folded, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Clamped, FoldAmount))
	{
		return;
	}
	FoldAmount = Clamped;

	UWidget* const Layout = GetLayoutWidget();

	// Shrink toward the anchor: a top-left element folds into the top-left corner, a full-screen one into the centre.
	FVector2D Pivot(0.5, 0.5);
	if (const UCanvasPanelSlot* const CanvasSlot = Cast<UCanvasPanelSlot>(Layout->Slot))
	{
		const FAnchors Anchors = CanvasSlot->GetAnchors();
		Pivot = (Anchors.Minimum + Anchors.Maximum) * 0.5;
	}

	const float Eased = FHudDraw::Ease(FoldAmount);
	const float Scale = FMath::Lerp(1.f, GetHudStyle().FoldedScale, Eased);
	Layout->SetRenderTransformPivot(Pivot);
	Layout->SetRenderScale(FVector2D(Scale, Scale));
	Layout->SetRenderOpacity(1.f - Eased);
}
