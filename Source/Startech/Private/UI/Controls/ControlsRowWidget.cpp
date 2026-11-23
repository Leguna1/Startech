#include "UI/Controls/ControlsRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "UI/Controls/ControlsKeyCapWidget.h"
#include "UI/HUD/StartechHudStyle.h"

#define LOCTEXT_NAMESPACE "ControlsRow"

namespace ControlsRow
{
	constexpr float KeyColumnWidth = 210.f;
	constexpr float NameColumnWidth = 170.f;
	constexpr float ColumnGap = 16.f;
	constexpr float CapGap = 4.f;
	constexpr float RowPadding = 6.f;
}

void UControlsRowWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UHorizontalBox* const Root = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = Root;

	USizeBox* const KeyColumn = WidgetTree->ConstructWidget<USizeBox>();
	KeyColumn->SetWidthOverride(ControlsRow::KeyColumnWidth);
	UHorizontalBoxSlot* const KeySlot = Root->AddChildToHorizontalBox(KeyColumn);
	KeySlot->SetVerticalAlignment(VAlign_Center);
	KeySlot->SetPadding(FMargin(0.f, ControlsRow::RowPadding, ControlsRow::ColumnGap, ControlsRow::RowPadding));

	KeyBox = WidgetTree->ConstructWidget<UWrapBox>();
	KeyBox->SetInnerSlotPadding(FVector2D(ControlsRow::CapGap, ControlsRow::CapGap));
	KeyColumn->AddChild(KeyBox);

	USizeBox* const NameColumn = WidgetTree->ConstructWidget<USizeBox>();
	NameColumn->SetWidthOverride(ControlsRow::NameColumnWidth);
	UHorizontalBoxSlot* const NameSlot = Root->AddChildToHorizontalBox(NameColumn);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(0.f, ControlsRow::RowPadding, ControlsRow::ColumnGap, ControlsRow::RowPadding));

	NameText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.TextColor, true);
	NameText->SetAutoWrapText(true);
	NameColumn->AddChild(NameText);

	DescriptionText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.TextSecondaryColor);
	DescriptionText->SetAutoWrapText(true);
	UHorizontalBoxSlot* const DescriptionSlot = Root->AddChildToHorizontalBox(DescriptionText);
	DescriptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DescriptionSlot->SetVerticalAlignment(VAlign_Center);
	DescriptionSlot->SetPadding(FMargin(0.f, ControlsRow::RowPadding));

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UControlsRowWidget::Setup(const FControlsListRow& Row)
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	NameText->SetText(Row.DisplayName);
	DescriptionText->SetText(Row.Description);
	bCombination = Row.bCombination;

	KeyBox->ClearChildren();
	Caps.Reset();
	CapDimmed.Reset();
	for (int32 Index = 0; Index < Row.Keys.Num(); ++Index)
	{
		if (Index > 0)
		{
			UTextBlock* const Separator = MakeText(bCombination ? LOCTEXT("Plus", "+") : LOCTEXT("Or", "/"),
				HudStyle.BodyFontSize, HudStyle.TextSecondaryColor);
			KeyBox->AddChildToWrapBox(Separator)->SetVerticalAlignment(VAlign_Center);
		}
		UControlsKeyCapWidget* const Cap = WidgetTree->ConstructWidget<UControlsKeyCapWidget>(UControlsKeyCapWidget::StaticClass());
		Cap->Setup(Row.Keys[Index].Text, Row.Keys[Index].bDimmed);
		KeyBox->AddChildToWrapBox(Cap)->SetVerticalAlignment(VAlign_Center);
		Caps.Add(Cap);
		CapDimmed.Add(Row.Keys[Index].bDimmed);
	}
}

FString UControlsRowWidget::DescribeKeys() const
{
	TArray<FString> Parts;
	for (int32 Index = 0; Index < Caps.Num(); ++Index)
	{
		const FString Label = Caps[Index]->GetLabel().ToString();
		Parts.Add(CapDimmed[Index] ? FString::Printf(TEXT("[%s]"), *Label) : Label);
	}
	return FString::Join(Parts, bCombination ? TEXT(" + ") : TEXT(" / "));
}

FText UControlsRowWidget::GetDisplayName() const
{
	return NameText ? NameText->GetText() : FText::GetEmpty();
}

FText UControlsRowWidget::GetDescription() const
{
	return DescriptionText ? DescriptionText->GetText() : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
