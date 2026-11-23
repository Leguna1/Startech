#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Controls/ControlsHelpData.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Flow/StartechCommandQueue.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "UI/Controls/ControlsRowWidget.h"
#include "UI/Controls/ControlsScreenWidget.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Menu/MainMenuWidget.h"
#include "UI/Pause/PauseMenuWidget.h"

/**
 * Controls screen test aids (non-shipping):
 *   Startech.Test.Controls [log|open|back]   log: which menu is up, whether the Controls screen is open, and every row
 *                                            as drawn (group | keys | name | description) and the tips; open: a real
 *                                            mouse click on the menu's Controls button; back: a real click on Back
 *   Startech.Test.Sequence <a;wait 2;b>      runs console commands spaced out in real time, as
 *                                            Startech.Flow.OnStartCommands does in the game map (works in the menu)
 */
namespace ControlsConsole
{
	/** The open main menu or pause menu's Controls screen and Controls button. */
	struct FMenu
	{
		const TCHAR* Name = TEXT("none");
		UControlsScreenWidget* Screen = nullptr;
		UHubButtonWidget* ControlsButton = nullptr;
	};

	static FMenu FindMenu(UWorld* const World)
	{
		FMenu Menu;
		if (!World)
		{
			return Menu;
		}
		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, UPauseMenuWidget::StaticClass(), false);
		for (UUserWidget* const Widget : Found)
		{
			if (const UPauseMenuWidget* const Pause = Cast<UPauseMenuWidget>(Widget); Pause && Pause->IsInViewport())
			{
				Menu.Name = TEXT("pause menu");
				Menu.Screen = Pause->GetControlsScreen();
				Menu.ControlsButton = Pause->GetControlsButton();
				return Menu;
			}
		}
		Found.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, UMainMenuWidget::StaticClass(), false);
		for (UUserWidget* const Widget : Found)
		{
			if (const UMainMenuWidget* const Main = Cast<UMainMenuWidget>(Widget); Main && Main->IsInViewport())
			{
				Menu.Name = TEXT("main menu");
				Menu.Screen = Main->GetControlsScreen();
				Menu.ControlsButton = Main->GetControlsButton();
				return Menu;
			}
		}
		return Menu;
	}

	/** A real left click through Slate at the widget's centre, routed like the mouse. */
	static bool Click(const UWidget& Widget)
	{
		const FGeometry& Geometry = Widget.GetCachedGeometry();
		if (!FSlateApplication::IsInitialized() || Geometry.GetAbsoluteSize().IsNearlyZero())
		{
			return false;
		}
		const FVector2D Centre = FVector2D(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f);
		FSlateApplication& Slate = FSlateApplication::Get();
		const FModifierKeysState NoModifiers;
		const TSet<FKey> NoButtons;
		const TSet<FKey> LeftButton = { EKeys::LeftMouseButton };
		Slate.SetCursorPos(Centre);
		Slate.ProcessMouseMoveEvent(FPointerEvent(0, Centre, Centre, NoButtons, EKeys::Invalid, 0.f, NoModifiers));
		Slate.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(0, Centre, Centre, LeftButton, EKeys::LeftMouseButton, 0.f, NoModifiers));
		Slate.ProcessMouseButtonUpEvent(FPointerEvent(0, Centre, Centre, NoButtons, EKeys::LeftMouseButton, 0.f, NoModifiers));
		return true;
	}

	static void Log(const FMenu& Menu)
	{
		const UControlsScreenWidget* const Screen = Menu.Screen;
		UE_LOG(LogStartechControls, Display, TEXT("[Controls] menu=%s screen=%s"), Menu.Name,
			!Screen ? TEXT("missing") : Screen->IsOpen() ? TEXT("OPEN") : TEXT("closed"));
		if (!Screen || !Screen->IsOpen())
		{
			return;
		}
		for (const FText& Tip : Screen->GetTipLines())
		{
			UE_LOG(LogStartechControls, Display, TEXT("[Controls] tip %s"), *Tip.ToString());
		}
		const TArray<TObjectPtr<UControlsRowWidget>>& Rows = Screen->GetRows();
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			UE_LOG(LogStartechControls, Display, TEXT("[Controls] row %d %s | %s | %s | %s"), Index,
				*Screen->GetRowGroups()[Index].ToString(), *Rows[Index]->DescribeKeys(),
				*Rows[Index]->GetDisplayName().ToString(), *Rows[Index]->GetDescription().ToString());
		}
		UE_LOG(LogStartechControls, Display, TEXT("[Controls] %d row(s)"), Rows.Num());
	}

	static void Controls(const TArray<FString>& Args, UWorld* const World)
	{
		const FMenu Menu = FindMenu(World);
		const FString Verb = Args.IsEmpty() ? TEXT("log") : Args[0];
		if (Verb.Equals(TEXT("open"), ESearchCase::IgnoreCase))
		{
			const bool bClicked = Menu.ControlsButton && Click(*Menu.ControlsButton);
			UE_LOG(LogStartechControls, Display, TEXT("[Controls] clicked Controls in the %s: %s"), Menu.Name,
				bClicked ? TEXT("yes") : TEXT("NO (no menu, or the button isn't on screen)"));
		}
		else if (Verb.Equals(TEXT("back"), ESearchCase::IgnoreCase))
		{
			const bool bClicked = Menu.Screen && Menu.Screen->GetBackButton() && Click(*Menu.Screen->GetBackButton());
			UE_LOG(LogStartechControls, Display, TEXT("[Controls] clicked Back in the %s: %s"), Menu.Name,
				bClicked ? TEXT("yes") : TEXT("NO (the Controls screen isn't on screen)"));
		}
		else
		{
			Log(Menu);
		}
	}

	static void Sequence(const TArray<FString>& Args, UWorld*)
	{
		// Never destroyed: it must outlive map travel, and the core ticker may be gone before static destructors run.
		static FStartechCommandQueue* const Queue = new FStartechCommandQueue();
		Queue->Start(FString::Join(Args, TEXT(" ")), []() -> UWorld*
		{
			return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		});
	}

	static FAutoConsoleCommandWithWorldAndArgs ControlsCommand(TEXT("Startech.Test.Controls"),
		TEXT("Startech.Test.Controls [log|open|back]: logs the Controls screen's rows, or clicks the menu's Controls button or Back."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Controls));

	static FAutoConsoleCommandWithWorldAndArgs SequenceCommand(TEXT("Startech.Test.Sequence"),
		TEXT("Startech.Test.Sequence <a;wait 2;b>: runs console commands 0.25 s apart in real time; 'wait <s>' delays the next."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Sequence));
}

#endif
