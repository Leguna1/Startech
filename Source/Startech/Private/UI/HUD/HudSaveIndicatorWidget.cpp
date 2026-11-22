#include "UI/HUD/HudSaveIndicatorWidget.h"

#include "Engine/GameInstance.h"
#include "Runtime/SaveSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

#define LOCTEXT_NAMESPACE "HudSaveIndicator"

namespace HudSaveIndicator
{
	/** "Saved" stays this long after a save completes. */
	constexpr double SavedSeconds = 1.0;

	/** "Save failed" stays this long. */
	constexpr double FailedSeconds = 5.0;
}

void UHudSaveIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (USaveSubsystem* const Saves = GetSaves())
	{
		Saves->OnSaveStarted.AddUniqueDynamic(this, &UHudSaveIndicatorWidget::HandleSaveStarted);
		Saves->OnSaveCompleted.AddUniqueDynamic(this, &UHudSaveIndicatorWidget::HandleSaveCompleted);
		SetState(Saves->IsSaveInFlight() ? EState::Saving : EState::Hidden, 0.0);
	}
}

void UHudSaveIndicatorWidget::NativeDestruct()
{
	if (USaveSubsystem* const Saves = GetSaves())
	{
		Saves->OnSaveStarted.RemoveDynamic(this, &UHudSaveIndicatorWidget::HandleSaveStarted);
		Saves->OnSaveCompleted.RemoveDynamic(this, &UHudSaveIndicatorWidget::HandleSaveCompleted);
	}
	Super::NativeDestruct();
}

void UHudSaveIndicatorWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if ((State == EState::Saved || State == EState::Failed) && GetHudTime() >= HideAt)
	{
		SetState(EState::Hidden, 0.0);
	}
}

int32 UHudSaveIndicatorWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	if (State == EState::Hidden)
	{
		return BaseLayer;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	FText Text;
	FLinearColor Color = HudStyle.TextSecondaryColor;
	switch (State)
	{
	case EState::Saving:
		Text = LOCTEXT("Saving", "Saving…");
		break;
	case EState::Saved:
		Text = LOCTEXT("Saved", "Saved");
		break;
	case EState::Failed:
		Text = LOCTEXT("Failed", "Save failed");
		Color = HudStyle.DangerColor;
		break;
	case EState::Hidden:
		break;
	}

	// Right-aligned and bottom-aligned, so it sits directly above the cargo and credits panel below it.
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	const FSlateFontInfo Font = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const float Height = FHudDraw::Measure(Text.ToString(), Font).Y;
	FHudDraw::Text(OutDrawElements, BaseLayer + 1, AllottedGeometry, Text.ToString(), Font,
		FVector2f(Size.X, FMath::Max(Size.Y - Height, 0.f)), Color, EHudTextAlign::Right);
	return BaseLayer + 1;
}

void UHudSaveIndicatorWidget::HandleSaveStarted(const FName Reason)
{
	SetState(EState::Saving, 0.0);
}

void UHudSaveIndicatorWidget::HandleSaveCompleted(const bool bSuccess, const FName Reason)
{
	if (bSuccess)
	{
		SetState(EState::Saved, HudSaveIndicator::SavedSeconds);
	}
	else
	{
		SetState(EState::Failed, HudSaveIndicator::FailedSeconds);
	}
}

void UHudSaveIndicatorWidget::SetState(const EState NewState, const double Seconds)
{
	// A failure stays up its full time; a later save's progress replaces it only once it is gone.
	if (State == EState::Failed && NewState == EState::Saving && GetHudTime() < HideAt)
	{
		return;
	}
	State = NewState;
	HideAt = GetHudTime() + Seconds;
	RequestRepaint();
}

USaveSubsystem* UHudSaveIndicatorWidget::GetSaves() const
{
	const UGameInstance* const GameInstance = IsDesignTime() ? nullptr : GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
