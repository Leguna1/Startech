#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HubWidgetBase.generated.h"

class AStartechShipPawn;
class UQuestSubsystem;
class UStartechHudStyle;
class UTextBlock;

/**
 * Base of every station hub panel. Like the HUD elements, a panel builds its own content in C++ (BuildContent, once,
 * when its Slate widget is first made), so the Widget Blueprint only places the panels. The style is this widget's
 * own, else the nearest enclosing hub widget's (UStationHubWidget, UMainMenuWidget; found through the outer chain,
 * which every widget inside a screen shares), else the style class defaults.
 */
UCLASS(Abstract)
class STARTECH_API UHubWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** This widget's style, else the enclosing screen's, else the style class defaults. Never null. */
	const UStartechHudStyle& GetHudStyle() const;

protected:
	/**
	 * Builds the content (BuildContent) as soon as the widget is created, so a panel's Setup can use its child widgets
	 * before it is shown.
	 */
	virtual void NativeOnInitialized() override;

	/** Builds the content with BuildContent the first time, when the widget tree has no root yet. */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/**
	 * Makes the widget tree in C++. Runs once, when the widget is created (or first shown, in the designer), so it must
	 * not bind to gameplay.
	 */
	virtual void BuildContent() {}

	/** The owning player's ship, or null. */
	AStartechShipPawn* GetShip() const;

	/** The game instance's quest subsystem, or null (for example in the designer). */
	UQuestSubsystem* GetQuests() const;

	/** A new text block in this widget's tree, in the HUD's Roboto at Size points. */
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold = false);

	/** Sets a text block's font and colour. */
	static void StyleText(UTextBlock& Text, int32 Size, const FLinearColor& Color, bool bBold = false);

	/** Optional: overrides the style the hub hands down, for this widget only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hub")
	TObjectPtr<UStartechHudStyle> Style;
};
