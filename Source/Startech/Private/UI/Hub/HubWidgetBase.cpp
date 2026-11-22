#include "UI/Hub/HubWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Runtime/QuestSubsystem.h"
#include "StartechShipPawn.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/StationHubWidget.h"

const UStartechHudStyle& UHubWidgetBase::GetHudStyle() const
{
	if (Style)
	{
		return *Style;
	}

	// Widgets placed in a screen (WBP_StationHub, WBP_MainMenu), and those its panels make at runtime, all live in that
	// screen's widget trees, so the nearest enclosing hub widget hands its style down.
	if (const UHubWidgetBase* const Enclosing = GetTypedOuter<UHubWidgetBase>())
	{
		return Enclosing->GetHudStyle();
	}
	return UStartechHudStyle::GetDefaultStyle();
}

void UHubWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildContent();
	}
}

TSharedRef<SWidget> UHubWidgetBase::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildContent();
	}
	return Super::RebuildWidget();
}

AStartechShipPawn* UHubWidgetBase::GetShip() const
{
	return Cast<AStartechShipPawn>(GetOwningPlayerPawn());
}

UQuestSubsystem* UHubWidgetBase::GetQuests() const
{
	const UGameInstance* const GameInstance = IsDesignTime() ? nullptr : GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr;
}

UTextBlock* UHubWidgetBase::MakeText(const FText& Text, const int32 Size, const FLinearColor& Color, const bool bBold)
{
	UTextBlock* const Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(Text);
	StyleText(*Block, Size, Color, bBold);
	return Block;
}

void UHubWidgetBase::StyleText(UTextBlock& Text, const int32 Size, const FLinearColor& Color, const bool bBold)
{
	Text.SetFont(UStartechHudStyle::MakeFont(Size, bBold));
	Text.SetColorAndOpacity(FSlateColor(Color));
}
