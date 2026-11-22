#include "Flow/StartechPauseMenuSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Flow/StartechInputState.h"
#include "Flow/StartechStartFlowSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Gates/WarpTransitionComponent.h"
#include "HealthComponent.h"
#include "HomeRespawnSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Save/StartechSaveLog.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "UI/Pause/PauseMenuWidget.h"

namespace StartechPause
{
	/** Above the HUD and the hub. */
	constexpr int32 ZOrder = 100;
}

UStartechPauseMenuSubsystem* UStartechPauseMenuSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UStartechPauseMenuSubsystem>() : nullptr;
}

bool UStartechPauseMenuSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UStartechPauseMenuSubsystem::CanPause() const
{
	const UWorld* const World = GetWorld();
	const UStartechStartFlowSubsystem* const StartFlow = World ? World->GetSubsystem<UStartechStartFlowSubsystem>() : nullptr;
	const UHomeRespawnSubsystem* const Respawn = World ? World->GetSubsystem<UHomeRespawnSubsystem>() : nullptr;
	const AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UWarpTransitionComponent* const Warp = Ship ? Ship->FindComponentByClass<UWarpTransitionComponent>() : nullptr;

	return Ship && StartFlow && StartFlow->HasStarted()
		&& !Ship->IsHubOpen()
		&& !(Ship->GetHealthComponent() && Ship->GetHealthComponent()->IsDead())
		&& !(Respawn && Respawn->IsRespawning())
		&& !(Warp && Warp->IsWarping());
}

bool UStartechPauseMenuSubsystem::OpenPauseMenu()
{
	if (IsPauseMenuOpen() || !CanPause())
	{
		return false;
	}

	APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0);
	const TSubclassOf<UPauseMenuWidget> MenuClass = UStartechSettings::Get().PauseMenuWidgetClass.LoadSynchronous();
	if (!PC || !MenuClass)
	{
		UE_LOG(LogStartechSave, Error, TEXT("UStartechSettings::PauseMenuWidgetClass is not set: no pause menu."));
		return false;
	}

	if (AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(PC->GetPawn()))
	{
		Ship->ClearHeldInput();
	}

	PauseMenu = CreateWidget<UPauseMenuWidget>(PC, MenuClass);
	PauseMenu->AddToViewport(StartechPause::ZOrder);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(PauseMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	UGameplayStatics::SetGamePaused(this, true);
	return true;
}

void UStartechPauseMenuSubsystem::Resume()
{
	if (!IsPauseMenuOpen())
	{
		return;
	}
	PauseMenu->RemoveFromParent();
	PauseMenu = nullptr;

	UGameplayStatics::SetGamePaused(this, false);
	if (APlayerController* const PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FStartechInputState::SetGameOnly(*PC);
	}
}

void UStartechPauseMenuSubsystem::Deinitialize()
{
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
	}
	Super::Deinitialize();
}
