#include "UI/Pause/PauseMenuWidget.h"

#include "Flow/StartechGameFlowSubsystem.h"
#include "Flow/StartechPauseMenuSubsystem.h"
#include "HomeRespawnSubsystem.h"
#include "StartechShipPawn.h"
#include "UI/Controls/ControlsScreenWidget.h"
#include "UI/Settings/SettingsScreenWidget.h"
#include "UI/Menu/MainMenuDialog.h"
#include "UI/Pause/PauseMenuPanel.h"

#define LOCTEXT_NAMESPACE "PauseMenu"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!ControlsScreen)
	{
		ControlsScreen = UMenuPageWidget::AddToMenu<UControlsScreenWidget>(*this, TEXT("ControlsScreen"));
	}
	if (!SettingsScreen)
	{
		SettingsScreen = UMenuPageWidget::AddToMenu<USettingsScreenWidget>(*this, TEXT("SettingsScreen"));
		if (SettingsScreen)
		{
			SettingsScreen->SetShowMenuMusic(false);
		}
	}
}

void UPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PausePanel->OnResume.AddUObject(this, &UPauseMenuWidget::HandleResume);
	PausePanel->OnControls.AddUObject(this, &UPauseMenuWidget::OpenControls);
	PausePanel->OnSettings.AddUObject(this, &UPauseMenuWidget::OpenSettings);
	if (ControlsScreen)
	{
		ControlsScreen->OnClosed.AddUObject(this, &UPauseMenuWidget::HandlePageClosed);
	}
	if (SettingsScreen)
	{
		SettingsScreen->OnClosed.AddUObject(this, &UPauseMenuWidget::HandlePageClosed);
	}
	PausePanel->OnQuitToMainMenu.AddUObject(this, &UPauseMenuWidget::HandleQuitToMainMenu);
	PausePanel->OnQuitGame.AddUObject(this, &UPauseMenuWidget::HandleQuitGame);
}

void UPauseMenuWidget::NativeDestruct()
{
	PausePanel->OnResume.RemoveAll(this);
	PausePanel->OnControls.RemoveAll(this);
	PausePanel->OnSettings.RemoveAll(this);
	if (ControlsScreen)
	{
		ControlsScreen->OnClosed.RemoveAll(this);
	}
	if (SettingsScreen)
	{
		SettingsScreen->OnClosed.RemoveAll(this);
	}
	PausePanel->OnQuitToMainMenu.RemoveAll(this);
	PausePanel->OnQuitGame.RemoveAll(this);
	Super::NativeDestruct();
}

FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Escape || InKeyEvent.IsRepeat())
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (UMenuPageWidget* const Page = GetOpenPage())
	{
		Page->Close();
	}
	else if (Dialog->IsOpen())
	{
		Dialog->Cancel();
	}
	else
	{
		HandleResume();
	}
	return FReply::Handled();
}

void UPauseMenuWidget::HandleResume()
{
	if (UStartechPauseMenuSubsystem* const PauseMenus = UStartechPauseMenuSubsystem::Get(this))
	{
		PauseMenus->Resume();
	}
}

void UPauseMenuWidget::OpenControls()
{
	OpenPage(ControlsScreen);
}

void UPauseMenuWidget::OpenSettings()
{
	OpenPage(SettingsScreen);
}

void UPauseMenuWidget::OpenPage(UMenuPageWidget* const Page)
{
	if (!Page || GetOpenPage())
	{
		return;
	}
	VisibilityBeforePage = PausePanel->GetVisibility();
	PausePanel->SetVisibility(ESlateVisibility::Collapsed);
	Page->Open();
	SetKeyboardFocus();
}

void UPauseMenuWidget::HandlePageClosed()
{
	PausePanel->SetVisibility(VisibilityBeforePage);
	SetKeyboardFocus();
}

UMenuPageWidget* UPauseMenuWidget::GetOpenPage() const
{
	if (ControlsScreen && ControlsScreen->IsOpen())
	{
		return ControlsScreen;
	}
	return SettingsScreen && SettingsScreen->IsOpen() ? SettingsScreen.Get() : nullptr;
}

UHubButtonWidget* UPauseMenuWidget::GetSettingsButton() const
{
	return PausePanel ? PausePanel->GetSettingsButton() : nullptr;
}

UHubButtonWidget* UPauseMenuWidget::GetControlsButton() const
{
	return PausePanel ? PausePanel->GetControlsButton() : nullptr;
}

void UPauseMenuWidget::HandleQuitToMainMenu()
{
	ConfirmQuit(LOCTEXT("QuitToMenu", "Quit to main menu"), [this]()
	{
		if (UStartechGameFlowSubsystem* const Flow = UStartechGameFlowSubsystem::Get(this))
		{
			Flow->QuitToMainMenu();
		}
	});
}

void UPauseMenuWidget::HandleQuitGame()
{
	ConfirmQuit(LOCTEXT("QuitGame", "Quit game"), [this]()
	{
		if (UStartechGameFlowSubsystem* const Flow = UStartechGameFlowSubsystem::Get(this))
		{
			Flow->QuitGameFromPlay();
		}
	});
}

void UPauseMenuWidget::ConfirmQuit(const FText& QuitLabel, TFunction<void()> Quit)
{
	const AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(GetOwningPlayerPawn());
	if (!Ship || Ship->IsDocked())
	{
		Quit();
		return;
	}

	const UWorld* const World = GetWorld();
	const UHomeRespawnSubsystem* const Respawn = World ? World->GetSubsystem<UHomeRespawnSubsystem>() : nullptr;
	const FText Home = Respawn ? Respawn->GetHomeDisplayName() : LOCTEXT("HomeFallback", "the home");
	Dialog->Ask(FText::Format(LOCTEXT("NotDocked",
		"You'll resume docked at {0} station. Cargo in space and unfinished trips are lost.\n\nYour cargo hold, credits and quests are kept."),
		Home), QuitLabel, MoveTemp(Quit));
}

#undef LOCTEXT_NAMESPACE
