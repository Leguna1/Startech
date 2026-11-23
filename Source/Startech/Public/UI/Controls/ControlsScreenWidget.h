#pragma once

#include "CoreMinimal.h"
#include "UI/Menu/MenuPageWidget.h"
#include "ControlsScreenWidget.generated.h"

class UControlsRowWidget;
class UHubButtonWidget;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/**
 * The Controls screen, a full-screen page (UMenuPageWidget) over the main menu and the pause menu. It lists every
 * input, grouped, from DA_ControlsHelp (UStartechSettings::ControlsHelp) with the keys the data's mapping contexts hold
 * when it opens (FControlsList), under a "How combat works" box. The list scrolls. Back or Escape closes it.
 */
UCLASS()
class STARTECH_API UControlsScreenWidget : public UMenuPageWidget
{
	GENERATED_BODY()

public:
	/** The Back button, for tests. */
	UHubButtonWidget* GetBackButton() const { return BackButton; }

	/** The rows in screen order, for logs and tests. */
	const TArray<TObjectPtr<UControlsRowWidget>>& GetRows() const { return Rows; }

	/** The heading each row in GetRows() is under. */
	const TArray<FText>& GetRowGroups() const { return RowGroups; }

	/** The tips box's lines as drawn. */
	TArray<FText> GetTipLines() const;

protected:
	/** The panel with the title, the tips box, the scrolling list and Back. */
	virtual void BuildContent() override;

	/** Binds Back. */
	virtual void NativeConstruct() override;

	/** Unbinds Back. */
	virtual void NativeDestruct() override;

	/** Rebuilds the list from the data and the current keys and scrolls to the top. */
	virtual void OnPageOpened() override;

private:
	/** Fills the tips box and the list from DA_ControlsHelp. */
	void Rebuild();

	/** The tips box's heading. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TipsTitle;

	/** The tips box's lines. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> TipsBox;

	/** Scrolls the list. */
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;

	/** The headings and rows. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ListBox;

	/** Back. */
	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> BackButton;

	/** One per row shown. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UControlsRowWidget>> Rows;

	/** The heading of each row in Rows. */
	TArray<FText> RowGroups;
};
