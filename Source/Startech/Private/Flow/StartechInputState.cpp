#include "Flow/StartechInputState.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void FStartechInputState::SetGameOnly(APlayerController& PC)
{
	PC.SetInputMode(FInputModeGameOnly());
	PC.SetShowMouseCursor(false);
	PC.FlushPressedKeys();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

FString FStartechInputState::Describe(const UWorld* const World)
{
	const APlayerController* const PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	const APawn* const Pawn = PC ? PC->GetPawn() : nullptr;
	// Not const: UGameViewportClient::IgnoreInput is a non-const getter.
	UGameViewportClient* const Viewport = World ? World->GetGameViewport() : nullptr;

	FString Focus = TEXT("none");
	if (FSlateApplication::IsInitialized())
	{
		if (const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0))
		{
			Focus = Focused->GetTypeAsString();
		}
	}

	return FString::Printf(TEXT("pc=%s pawn=%s possessed=%s pawnInput=%s viewportIgnoresInput=%s capture=%s cursor=%s focus=%s"),
		*GetNameSafe(PC), *GetNameSafe(Pawn), Pawn && Pawn->GetController() == PC ? TEXT("yes") : TEXT("no"),
		Pawn ? (Pawn->InputEnabled() ? TEXT("on") : TEXT("OFF")) : TEXT("-"),
		Viewport ? (Viewport->IgnoreInput() ? TEXT("YES") : TEXT("no")) : TEXT("-"),
		Viewport ? *StaticEnum<EMouseCaptureMode>()->GetNameStringByValue(static_cast<int64>(Viewport->GetMouseCaptureMode())) : TEXT("-"),
		PC ? (PC->ShouldShowMouseCursor() ? TEXT("on") : TEXT("off")) : TEXT("-"), *Focus);
}
