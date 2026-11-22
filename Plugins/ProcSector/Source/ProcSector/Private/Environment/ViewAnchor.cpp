#include "Environment/ViewAnchor.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

FVector FViewAnchor::GetViewLocation(const UWorld* World)
{
	const APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (PlayerController != nullptr && PlayerController->PlayerCameraManager != nullptr)
	{
		return PlayerController->PlayerCameraManager->GetCameraLocation();
	}
	return FVector::ZeroVector;
}
