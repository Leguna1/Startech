#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StartechMenuGameMode.generated.h"

class UMainMenuWidget;

/**
 * The main menu map's game mode (set as L_MainMenu's GameMode Override). No pawn and no HUD: it shows
 * UStartechSettings::MainMenuWidgetClass (WBP_MainMenu) with the mouse cursor, UI-only input. The game map's start
 * flow (UStartechStartFlowSubsystem) stays off in a world run by this mode.
 */
UCLASS()
class STARTECH_API AStartechMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** No default pawn. */
	AStartechMenuGameMode();

	/** Shows the main menu for the first local player. */
	virtual void StartPlay() override;

private:
	/** The menu shown. */
	UPROPERTY(Transient)
	TObjectPtr<UMainMenuWidget> MainMenu;
};
