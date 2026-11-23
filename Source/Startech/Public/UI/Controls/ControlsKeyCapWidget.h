#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "ControlsKeyCapWidget.generated.h"

class UHubPanelWidget;
class UTextBlock;

/**
 * One small keycap of the Controls screen: a cut-corner panel (DA_HudStyle) around the key's label. Dimmed for an
 * input with no key ("Auto", "Fly in", "Unbound").
 */
UCLASS()
class STARTECH_API UControlsKeyCapWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Shows Label, dimmed or not. */
	void Setup(const FText& Label, bool bDimmed);

	/** The label as drawn, for logs and tests. */
	FText GetLabel() const;

protected:
	/** A size box holding the panel and the label. */
	virtual void BuildContent() override;

private:
	/** The panel behind the label. */
	UPROPERTY(Transient)
	TObjectPtr<UHubPanelWidget> Panel;

	/** The key's label. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text;
};
