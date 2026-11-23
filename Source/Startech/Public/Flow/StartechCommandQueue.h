#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class UWorld;

/**
 * Test aid: runs console commands one after another, 0.25 s apart in real time (so it keeps going while paused), as if
 * typed into the player's console. "wait <s>" only delays the next command. Used by Startech.Flow.OnStartCommands and
 * Startech.Test.Sequence. Does nothing in shipping builds.
 */
class STARTECH_API FStartechCommandQueue
{
public:
	/** Removes the pending tick. */
	~FStartechCommandQueue();

	/** Replaces any pending commands with Commands (separated by ';') and runs the first now. GetWorld is asked for
	 * the world at each command, so the queue can outlive a map. */
	void Start(const FString& Commands, TFunction<UWorld*()> InGetWorld);

	/** Drops the pending commands. */
	void Stop();

	/** Seconds between two commands. */
	static constexpr float SpacingSeconds = 0.25f;

private:
	/** Runs one command, then waits for the next. */
	void RunNext();

	TArray<FString> Pending;
	TFunction<UWorld*()> GetWorld;
	FTSTicker::FDelegateHandle Ticker;
};
