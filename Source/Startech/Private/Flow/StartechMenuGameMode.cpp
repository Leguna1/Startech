#include "Flow/StartechMenuGameMode.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Save/StartechSaveLog.h"
#include "StartechSettings.h"
#include "UI/Menu/MainMenuWidget.h"

AStartechMenuGameMode::AStartechMenuGameMode()
{
	DefaultPawnClass = nullptr;
}

void AStartechMenuGameMode::StartPlay()
{
	Super::StartPlay();

	APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0);
	const TSubclassOf<UMainMenuWidget> MenuClass = UStartechSettings::Get().MainMenuWidgetClass.LoadSynchronous();
	if (!PC || !MenuClass)
	{
		UE_LOG(LogStartechSave, Error, TEXT("No player controller, or UStartechSettings::MainMenuWidgetClass is not set: no main menu."));
		return;
	}

	MainMenu = CreateWidget<UMainMenuWidget>(PC, MenuClass);
	MainMenu->AddToViewport();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}
