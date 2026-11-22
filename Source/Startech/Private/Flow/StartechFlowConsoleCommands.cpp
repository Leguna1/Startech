#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "Flow/StartechGameFlowSubsystem.h"
#include "Flow/StartechInputState.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Save/StartechSaveLog.h"
#include "StartechShipPawn.h"
#include "Station.h"

/**
 * Start-flow debug commands (non-shipping). The Startech.Menu.* commands call exactly what the main menu's buttons
 * call, so the menu-to-game path can be run headless from L_MainMenu.
 *   Startech.Menu.NewGame <Slot> <Callsign>   New game > slot > callsign > Start
 *   Startech.Menu.Continue                    Continue
 *   Startech.Menu.Load <Slot>                 Load > slot
 *   Startech.Menu.QuitToMainMenu / QuitGame   the pause menu's quit buttons (after any warning)
 *   Startech.Test.Undock / Startech.Test.Dock undock as the hub does; move to the station's respawn point
 *   Startech.Flow.InputState                  logs who is possessed, pawn input, viewport input, cursor and focus
 *   Startech.Test.Hub <1|0> [Tab]             opens or closes the station hub (docked only); Tab (Quests, Shop,
 *                                             Upgrades) is the tab it opens on, unless a quest is ready to turn in
 *   Startech.Test.KeyDown|KeyUp <Key>         a key event through Slate, routed like the real keyboard
 */
namespace StartechFlowConsole
{
	static UStartechGameFlowSubsystem* GetFlow(const UWorld* const World)
	{
		UStartechGameFlowSubsystem* const Flow = UStartechGameFlowSubsystem::Get(World);
		if (!Flow)
		{
			UE_LOG(LogStartechSave, Warning, TEXT("No game flow subsystem."));
		}
		return Flow;
	}

	static void NewGame(const TArray<FString>& Args, UWorld* const World)
	{
		UStartechGameFlowSubsystem* const Flow = GetFlow(World);
		if (!Flow || Args.Num() < 2)
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Usage: Startech.Menu.NewGame <Slot> <Callsign>"));
			return;
		}
		FText Reason;
		if (!Flow->StartNewGame(FCString::Atoi(*Args[0]), FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" ")), Reason))
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Startech.Menu.NewGame: %s"), *Reason.ToString());
		}
	}

	static void Continue(const TArray<FString>& Args, UWorld* const World)
	{
		FText Reason;
		UStartechGameFlowSubsystem* const Flow = GetFlow(World);
		if (Flow && !Flow->ContinueGame(Reason))
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Startech.Menu.Continue: %s"), *Reason.ToString());
		}
	}

	static void Load(const TArray<FString>& Args, UWorld* const World)
	{
		FText Reason;
		UStartechGameFlowSubsystem* const Flow = GetFlow(World);
		if (Flow && !Args.IsEmpty() && !Flow->LoadGame(FCString::Atoi(*Args[0]), Reason))
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Startech.Menu.Load: %s"), *Reason.ToString());
		}
	}

	static void InputState(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		UE_LOG(LogStartechSave, Display, TEXT("[Input] %s hub=%s"), *FStartechInputState::Describe(World),
			Ship ? (Ship->IsHubOpen() ? TEXT("OPEN") : TEXT("closed")) : TEXT("-"));
	}

	static void Hub(const TArray<FString>& Args, UWorld* const World)
	{
		AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		if (!Ship)
		{
			return;
		}
		if (!Args.IsEmpty() && FCString::Atoi(*Args[0]) != 0)
		{
			const int64 Tab = Args.Num() > 1 ? StaticEnum<EHubTab>()->GetValueByNameString(Args[1]) : INDEX_NONE;
			if (Tab != INDEX_NONE)
			{
				Ship->SetLastHubTab(static_cast<EHubTab>(Tab));
			}
			Ship->OpenHub();
		}
		else
		{
			Ship->CloseHub();
		}
		UE_LOG(LogStartechSave, Display, TEXT("Startech.Test.Hub: hub %s"), Ship->IsHubOpen() ? TEXT("open") : TEXT("closed"));
	}

	/** Sends Key down (bDown) or up through Slate, as the keyboard would: to the focused widget, else the game viewport. */
	static void SendKey(const TArray<FString>& Args, const bool bDown)
	{
		const FKey Key(Args.IsEmpty() ? NAME_None : FName(*Args[0]));
		if (!Key.IsValid() || !FSlateApplication::IsInitialized())
		{
			UE_LOG(LogStartechSave, Warning, TEXT("Usage: Startech.Test.KeyDown|KeyUp <KeyName>, e.g. F or Escape."));
			return;
		}
		const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
		const bool bHandled = bDown ? FSlateApplication::Get().ProcessKeyDownEvent(Event) : FSlateApplication::Get().ProcessKeyUpEvent(Event);
		UE_LOG(LogStartechSave, Display, TEXT("Startech.Test.Key%s %s: %s"), bDown ? TEXT("Down") : TEXT("Up"), *Key.ToString(),
			bHandled ? TEXT("handled") : TEXT("not handled"));
	}

	static void KeyDown(const TArray<FString>& Args, UWorld* const World) { SendKey(Args, true); }
	static void KeyUp(const TArray<FString>& Args, UWorld* const World) { SendKey(Args, false); }

	static FAutoConsoleCommandWithWorldAndArgs KeyDownCommand(TEXT("Startech.Test.KeyDown"),
		TEXT("Startech.Test.KeyDown <Key>: a key press through Slate, as from the keyboard."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KeyDown));

	static FAutoConsoleCommandWithWorldAndArgs KeyUpCommand(TEXT("Startech.Test.KeyUp"),
		TEXT("Startech.Test.KeyUp <Key>: a key release through Slate."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KeyUp));

	static void QuitToMainMenu(const TArray<FString>& Args, UWorld* const World)
	{
		if (UStartechGameFlowSubsystem* const Flow = GetFlow(World))
		{
			Flow->QuitToMainMenu();
		}
	}

	static void QuitGame(const TArray<FString>& Args, UWorld* const World)
	{
		if (UStartechGameFlowSubsystem* const Flow = GetFlow(World))
		{
			Flow->QuitGameFromPlay();
		}
	}

	static void Undock(const TArray<FString>& Args, UWorld* const World)
	{
		if (AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr)
		{
			Ship->UndockFromStation();
		}
	}

	static void Dock(const TArray<FString>& Args, UWorld* const World)
	{
		AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		const AStation* const Station = Ship ? AStation::FindNearestInPlay(*World, Ship->GetActorLocation()) : nullptr;
		if (Station)
		{
			Ship->PlaceAt(Station->GetRespawnTransform());
		}
		UE_LOG(LogStartechSave, Display, TEXT("Startech.Test.Dock: %s"), Ship && Ship->IsDocked() ? TEXT("docked") : TEXT("not docked"));
	}

	static FAutoConsoleCommandWithWorldAndArgs QuitToMainMenuCommand(TEXT("Startech.Menu.QuitToMainMenu"),
		TEXT("What the pause menu's Quit to main menu does (after any warning)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&QuitToMainMenu));

	static FAutoConsoleCommandWithWorldAndArgs QuitGameCommand(TEXT("Startech.Menu.QuitGame"),
		TEXT("What the pause menu's Quit game does (after any warning)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&QuitGame));

	static FAutoConsoleCommandWithWorldAndArgs UndockCommand(TEXT("Startech.Test.Undock"),
		TEXT("Undocks the ship, as the hub's Undock button."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Undock));

	static FAutoConsoleCommandWithWorldAndArgs DockCommand(TEXT("Startech.Test.Dock"),
		TEXT("Moves the ship to the nearest station's respawn point, docking it."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Dock));

	static FAutoConsoleCommandWithWorldAndArgs NewGameCommand(TEXT("Startech.Menu.NewGame"),
		TEXT("Startech.Menu.NewGame <Slot> <Callsign>: what the menu's Start does."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&NewGame));

	static FAutoConsoleCommandWithWorldAndArgs ContinueCommand(TEXT("Startech.Menu.Continue"),
		TEXT("What the menu's Continue does."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Continue));

	static FAutoConsoleCommandWithWorldAndArgs LoadCommand(TEXT("Startech.Menu.Load"),
		TEXT("Startech.Menu.Load <Slot>: what the menu's Load does."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Load));

	static FAutoConsoleCommandWithWorldAndArgs InputStateCommand(TEXT("Startech.Flow.InputState"),
		TEXT("Logs possession, pawn input, the game viewport's input state, the cursor and Slate focus."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&InputState));

	static FAutoConsoleCommandWithWorldAndArgs HubCommand(TEXT("Startech.Test.Hub"),
		TEXT("Startech.Test.Hub <1|0>: opens or closes the station hub (docked only)."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Hub));
}

#endif
