#include "Flow/StartechCommandQueue.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

FStartechCommandQueue::~FStartechCommandQueue()
{
	Stop();
}

void FStartechCommandQueue::Start(const FString& Commands, TFunction<UWorld*()> InGetWorld)
{
#if !UE_BUILD_SHIPPING
	Stop();
	Commands.ParseIntoArray(Pending, TEXT(";"));
	GetWorld = MoveTemp(InGetWorld);
	RunNext();
#endif
}

void FStartechCommandQueue::Stop()
{
	Pending.Reset();
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
	}
}

void FStartechCommandQueue::RunNext()
{
#if !UE_BUILD_SHIPPING
	if (Pending.IsEmpty() || !GEngine)
	{
		return;
	}
	const FString Command = Pending[0].TrimStartAndEnd();
	Pending.RemoveAt(0);

	// "wait <seconds>" only delays the next command.
	float Delay = SpacingSeconds;
	if (Command.StartsWith(TEXT("wait "), ESearchCase::IgnoreCase))
	{
		Delay = FMath::Max(FCString::Atof(*Command.Mid(5)), SpacingSeconds);
	}
	else if (!Command.IsEmpty())
	{
		// Through the player's console when there is one, as if typed, so viewport commands (HighResShot) work too.
		UWorld* const World = GetWorld ? GetWorld() : nullptr;
		if (APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			PC->ConsoleCommand(Command);
		}
		else
		{
			GEngine->Exec(World, *Command);
		}
	}

	// Spaced out, so each command sees what the previous one caused (Slate applies focus changes a frame later).
	// Real time, so it also advances while the game is paused. A command above may have stopped the queue (quit).
	if (Pending.IsEmpty())
	{
		return;
	}
	if (Ticker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
	}
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
	{
		Ticker.Reset();
		RunNext();
		return false;
	}), Delay);
#endif
}
