#include "Debug/SectorDebugSubsystem.h"

#include "Core/SectorDirectorSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Population/ActorPoolSubsystem.h"
#include "Population/RelevanceSubsystem.h"

static TAutoConsoleVariable<int32> CVarProcSectorDebug(
	TEXT("ProcSector.Debug"),
	0,
	TEXT("1 shows the ProcSector overlay (sector, seed, play area, generation timings, pool and relevance counts). 2 also draws the play-area box. 0 hides it."),
	ECVF_Default);

void USectorDebugSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	const int32 DebugLevel = CVarProcSectorDebug.GetValueOnGameThread();
	if (World == nullptr || DebugLevel <= 0)
	{
		return;
	}

	int32 Line = 0;

	if (const USectorDirectorSubsystem* Director = World->GetSubsystem<USectorDirectorSubsystem>())
	{
		const TCHAR* PhaseText = TEXT("Idle");
		if (Director->GetPhase() == ESectorPhase::TearingDown)
		{
			PhaseText = TEXT("Tearing down");
		}
		else if (Director->GetPhase() == ESectorPhase::Generating)
		{
			PhaseText = TEXT("Generating");
		}

		PrintLine(Line++, FString::Printf(TEXT("Sector: %s   Seed: %d   Phase: %s"),
			*Director->GetCurrentSectorId().ToString(), Director->GetCurrentSeed(), PhaseText), FColor::Cyan);

		// The extent is a half-size in centimetres; the overlay shows the full size in metres.
		const FVector PlayArea = Director->GetPlayAreaExtent();
		const FVector FullSizeMetres = PlayArea * 2.0 / 100.0;
		PrintLine(Line++, FString::Printf(TEXT("Play area: %.0f x %.0f x %.0f m"), FullSizeMetres.X, FullSizeMetres.Y, FullSizeMetres.Z), FColor::Cyan);

		if (DebugLevel >= PlayAreaBoxLevel)
		{
			// Lifetime -1 with no persistence draws for exactly one frame; the box is redrawn every tick.
			DrawDebugBox(World, FVector::ZeroVector, PlayArea, FColor::Orange, false, -1.0f, SDPG_World, 200.0f);
		}

		PrintLine(Line++, FString::Printf(TEXT("Last generation: %.2f ms of work"), Director->GetLastGenerationMs()), FColor::White);

		for (const FSectorStepTiming& Timing : Director->GetLastStepTimings())
		{
			PrintLine(Line++, FString::Printf(TEXT("   %s: %.2f ms"), *Timing.StepName.ToString(), Timing.Milliseconds), FColor::Silver);
		}

		PrintLine(Line++, FString::Printf(TEXT("Respawns pending: %d"), Director->CountPendingRespawns()), FColor::White);
	}

	if (const UActorPoolSubsystem* Pool = World->GetSubsystem<UActorPoolSubsystem>())
	{
		PrintLine(Line++, FString::Printf(TEXT("Pool: %d active, %d stored"), Pool->GetActiveCount(), Pool->GetFreeCount()), FColor::Green);
	}

	if (const URelevanceSubsystem* Relevance = World->GetSubsystem<URelevanceSubsystem>())
	{
		PrintLine(Line++, FString::Printf(TEXT("Relevance: %d awake of %d tracked"), Relevance->GetAwakeCount(), Relevance->GetTrackedCount()), FColor::Yellow);
	}
}

TStatId USectorDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USectorDebugSubsystem, STATGROUP_Tickables);
}

bool USectorDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USectorDebugSubsystem::PrintLine(const int32 LineIndex, const FString& Text, const FColor& Color)
{
	if (GEngine != nullptr)
	{
		// A short lifetime with a fixed key: the line is refreshed every frame and vanishes when the overlay is turned off.
		GEngine->AddOnScreenDebugMessage(FirstMessageKey + LineIndex, 0.2f, Color, Text);
	}
}
