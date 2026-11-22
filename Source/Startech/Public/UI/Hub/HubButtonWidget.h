#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericApplication.h"
#include "UI/HUD/HudPanelBrush.h"
#include "UI/Hub/HubWidgetBase.h"
#include "HubButtonWidget.generated.h"

class USizeBox;

/** Broadcast when a hub button is clicked while enabled. GetLastClickModifiers says which modifier keys were held. */
DECLARE_MULTICAST_DELEGATE(FOnHubButtonClicked);

/** The look of a hub button. */
UENUM(BlueprintType)
enum class EHubButtonVariant : uint8
{
	/** The main action (buy, upgrade, accept): a Player-colour fill and outline. */
	Primary,
	/** A lesser action (sell, cancel): a quiet fill with a grey outline, Player fill under the mouse. */
	Secondary,
	/** One of a set (quantity chips, shop tabs): looks Secondary, and Primary-bright with a Player outline when selected. */
	Toggle,
	/** Text only, no panel (links like "Show completed"). */
	Quiet,
};

/**
 * The one styled button of the hub, the menus and the shop, painted in C++ (no UButton inside). Every colour and size
 * comes from DA_HudStyle (UStartechHudStyle): the cut-corner panel (M_HUD_ChamferPanel), the fill at rest, under the
 * mouse and while held (Button*FillColor), a Toggle's selected fill (ButtonSelectedFillColor), the Secondary look
 * (SecondaryButton*Color), the disabled opacity (ButtonDisabledOpacity), the height and padding.
 *
 * Content: a label, optionally followed by " · " and an accent in its own colour ("Buy 100 · 1,500 cr", the credits in
 * the Credits colour), and an optional second line in the small font under it (a reason or a price). The button sizes
 * itself to its text unless SetFixedWidth is used. Disabled, it draws faded and ignores the mouse.
 */
UCLASS()
class STARTECH_API UHubButtonWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse. */
	UHubButtonWidget(const FObjectInitializer& ObjectInitializer);

	/** The label, in sentence case. Resizes the button. */
	void SetLabel(const FText& InLabel);

	/** Text drawn after the label, separated by " · ", in Color. Empty for none. */
	void SetAccent(const FText& InAccent, const FLinearColor& InColor);

	/** A second, smaller line under the label, in Color. Empty for none; the button grows to fit it. */
	void SetSecondLine(const FText& InSecondLine, const FLinearColor& InColor);

	/** Enables or disables the button. A disabled button draws faded and is not clickable. */
	void SetButtonEnabled(bool bInEnabled);

	/** True if clickable. */
	bool IsButtonEnabled() const { return bEnabled; }

	/** The first line as drawn ("Buy 34 · 408 cr"), for logs and tests. */
	FString GetDisplayText() const { return GetFirstLine(); }

	/** The look. Primary by default. */
	void SetVariant(EHubButtonVariant InVariant);

	/** Old name for SetVariant(Quiet) / SetVariant(Primary). */
	void SetQuiet(bool bInQuiet) { SetVariant(bInQuiet ? EHubButtonVariant::Quiet : EHubButtonVariant::Primary); }

	/** A Toggle's selected state; ignored by the other variants. */
	void SetSelected(bool bInSelected);

	/** True when selected. */
	bool IsSelected() const { return bSelected; }

	/** Gives the button this width whatever its text, so a column of buttons lines up. 0 sizes it to its text. */
	void SetFixedWidth(float InWidth);

	/** The modifier keys held at the last click, for Shift-click shortcuts. */
	const FModifierKeysState& GetLastClickModifiers() const { return LastClickModifiers; }

	/** Fires on a click: press and release over the enabled button. */
	FOnHubButtonClicked OnClicked;

protected:
	/** A size box giving the button its size. */
	virtual void BuildContent() override;

	/** Makes the panel material instance. */
	virtual void NativeConstruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	/** The first line as one string: the label, plus " · " and the accent when there is one. */
	FString GetFirstLine() const;

	/** Sets the size box from the text's measured size, or the fixed width. */
	void UpdateSize();

	/** Sets the panel fill and outline for the variant, selection, hover and press state, and repaints. */
	void UpdateState();

	/** The label. */
	FText Label;

	/** Drawn after the label, or empty. */
	FText Accent;

	/** The accent's colour. */
	FLinearColor AccentColor = FLinearColor::White;

	/** The second line, or empty. */
	FText SecondLine;

	/** The second line's colour. */
	FLinearColor SecondLineColor = FLinearColor::White;

	/** The look. */
	EHubButtonVariant Variant = EHubButtonVariant::Primary;

	/** A Toggle's selected state. */
	bool bSelected = false;

	/** 0, or the width to use whatever the text. */
	float FixedWidth = 0.f;

	/** Clickable. */
	bool bEnabled = true;

	/** The mouse is over the button. */
	bool bHovered = false;

	/** The left button went down on it and has not been released. */
	bool bPressed = false;

	/** Modifier keys at the last click. */
	FModifierKeysState LastClickModifiers;

	/** The panel background of every variant but Quiet. */
	UPROPERTY(Transient)
	FHudPanelBrush Panel;

	/** Root of the content: sets the button's size. */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SizeBox;
};
