#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

/**
 * The single place that hands input between UI and game for Startech's screens and map travel, plus a readable report
 * of the input state for logs and the Startech.Flow.InputState command.
 */
struct STARTECH_API FStartechInputState
{
	/**
	 * Gives PC's input back to the game: game-only input mode (which also stops the game viewport ignoring input),
	 * hidden cursor, no keys held over from the UI, and keyboard focus on the game viewport.
	 */
	static void SetGameOnly(APlayerController& PC);

	/**
	 * "pawn=BP_ShipPawn_C_0 possessed=yes pawnInput=on viewportIgnoresInput=no capture=CapturePermanently cursor=off
	 * focus=SViewport": who is controlled, whether the pawn takes input, the game viewport's state and Slate's focus.
	 */
	static FString Describe(const UWorld* World);
};
