#include "Core/SectorDirectorSubsystem.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProcSectorLog.h"

/**
 * Console commands (Command pattern: each command is a small object bound to one action).
 * Open the console in Play mode with the tilde key.
 *   ProcSector.Goto <SectorId>   regenerate as that sector, without the warp sequence
 *   ProcSector.Regenerate        rebuild the current sector with the same seed
 *   ProcSector.RerollSeeds       new random seeds for every sector, state forgotten
 *   ProcSector.List              log every sector id in the catalog
 */
namespace ProcSectorConsole
{
	static USectorDirectorSubsystem* GetDirector(UWorld* World)
	{
		return World != nullptr ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	}

	static void Goto(const TArray<FString>& Args, UWorld* World)
	{
		USectorDirectorSubsystem* Director = GetDirector(World);
		if (Director == nullptr || Args.IsEmpty())
		{
			UE_LOG(LogProcSector, Warning, TEXT("Usage: ProcSector.Goto <SectorId> (only in a running game world)."));
			return;
		}
		Director->RequestSector(FName(*Args[0]));
	}

	static void Regenerate(const TArray<FString>& Args, UWorld* World)
	{
		if (USectorDirectorSubsystem* Director = GetDirector(World))
		{
			Director->RegenerateCurrentSector();
		}
	}

	static void RerollSeeds(const TArray<FString>& Args, UWorld* World)
	{
		if (USectorDirectorSubsystem* Director = GetDirector(World))
		{
			Director->RerollSeeds();
		}
	}

	static void List(const TArray<FString>& Args, UWorld* World)
	{
		USectorDirectorSubsystem* Director = GetDirector(World);
		const USectorCatalog* Catalog = Director != nullptr ? Director->GetCatalog() : nullptr;
		if (Catalog == nullptr)
		{
			return;
		}

		for (const FSectorDefinition& Sector : Catalog->Sectors)
		{
			UE_LOG(LogProcSector, Display, TEXT("Sector '%s' (seed %d, %d gates)"), *Sector.SectorId.ToString(), Sector.Seed, Sector.Gates.Num());
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GotoCommand(
		TEXT("ProcSector.Goto"),
		TEXT("Regenerates the level as the given sector. Usage: ProcSector.Goto <SectorId>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Goto));

	static FAutoConsoleCommandWithWorldAndArgs RegenerateCommand(
		TEXT("ProcSector.Regenerate"),
		TEXT("Rebuilds the current sector with the same seed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Regenerate));

	static FAutoConsoleCommandWithWorldAndArgs RerollSeedsCommand(
		TEXT("ProcSector.RerollSeeds"),
		TEXT("Gives every sector a new random seed, forgets sector state and regenerates."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RerollSeeds));

	static FAutoConsoleCommandWithWorldAndArgs ListCommand(
		TEXT("ProcSector.List"),
		TEXT("Logs every sector id in the catalog."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&List));
}
