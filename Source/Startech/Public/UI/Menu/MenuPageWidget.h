#pragma once

#include "CoreMinimal.h"
#include "UI/Hub/HubWidgetBase.h"
#include "MenuPageWidget.generated.h"

/**
 * A full-screen page shown over the main menu or the pause menu (the Controls screen, the Settings page). The menu
 * makes it in C++ with AddToMenu, so no Blueprint places it; it starts collapsed. The menu's button calls the menu's
 * OpenPage, which hides the menu's own page and Opens this one. Back or Escape Close it and fire OnClosed, and the menu
 * shows its page again. While a page is open the menu sends Escape to it first.
 */
UCLASS(Abstract)
class STARTECH_API UMenuPageWidget : public UHubWidgetBase
{
	GENERATED_BODY()

public:
	/** Takes the mouse and keyboard focus; starts collapsed. */
	UMenuPageWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Makes a collapsed page of Class in Menu's widget tree and lays it full screen, on top, on Menu's root canvas
	 * (WBP_MainMenu, WBP_PauseMenu). Call from NativeOnInitialized. Null, with an error logged, when the root is not a
	 * canvas panel.
	 */
	static UMenuPageWidget* AddToMenu(UUserWidget& Menu, TSubclassOf<UMenuPageWidget> Class, FName Name);

	template <typename T>
	static T* AddToMenu(UUserWidget& Menu, const FName Name) { return Cast<T>(AddToMenu(Menu, T::StaticClass(), Name)); }

	/** Shows the page (OnPageOpened first). Does nothing when it is already open. */
	void Open();

	/** Hides the page (OnPageClosing first) and fires OnClosed. Does nothing when it is not open. */
	void Close();

	/** True while shown. */
	bool IsOpen() const { return bOpen; }

	/** Fires when Back or Escape closed the page. */
	FSimpleMulticastDelegate OnClosed;

protected:
	/** Fill the page from current data: it is about to be shown. */
	virtual void OnPageOpened() {}

	/** The page is about to be hidden (save what it changed). */
	virtual void OnPageClosing() {}

	/** Escape closes the page. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	bool bOpen = false;
};
