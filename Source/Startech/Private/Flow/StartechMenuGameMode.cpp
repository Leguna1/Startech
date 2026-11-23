#include "Flow/StartechMenuGameMode.h"

#include "Audio/StartechMusicSubsystem.h"
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

	// After any game music fades out (quitting to the menu), the menu's playlist fades in.
	if (UStartechMusicSubsystem* const Music = UStartechMusicSubsystem::Get(this))
	{
		Music->PlayMenuMusic();
	}

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
