#include "CoreMinimal.h"
#include "Combat/CombatBalance.h"
#include "Combat/CombatTTKTable.h"
#include "Combat/CombatTelemetrySubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

/**
 * Combat balance console commands (non-shipping):
 *   Startech.Combat.TTKTable    prints the time-to-kill, damage and ammo-cost table from the live data ('*' = outside
 *                               its target). Needs no world: run it headless with
 *                               UnrealEditor-Cmd <uproject> -ExecCmds="Startech.Combat.TTKTable, QUIT_EDITOR" -nullrhi
 *   Startech.Combat.Report      prints the combat log's session summary per enemy type and tier (game worlds).
 */

#if !UE_BUILD_SHIPPING

DEFINE_LOG_CATEGORY_STATIC(LogStartechCombat, Log, All);

namespace StartechCombatConsole
{
	static void PrintTable()
	{
		const UCombatBalance* const Balance = UCombatBalance::Get();
		if (!Balance)
		{
			UE_LOG(LogStartechCombat, Warning, TEXT("[TTK] DA_CombatBalance is not set."));
			return;
		}
		for (const FString& Line : FCombatTTKTable::Build(*Balance).ToLines())
		{
			// Warning so it reaches stdout in a commandlet or -nullrhi run.
			UE_LOG(LogStartechCombat, Warning, TEXT("[TTK] %s"), *Line);
		}
	}

	static void PrintReport(const TArray<FString>& Args, UWorld* const World)
	{
		const UCombatTelemetrySubsystem* const Telemetry = UCombatTelemetrySubsystem::Get(World);
		if (!Telemetry)
		{
			UE_LOG(LogStartechCombat, Warning, TEXT("[CombatReport] No combat log in this world (not a game world, or shipping)."));
			return;
		}
		for (const FString& Line : Telemetry->BuildReport())
		{
			UE_LOG(LogStartechCombat, Warning, TEXT("[CombatReport] %s"), *Line);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs ReportCommand(TEXT("Startech.Combat.Report"),
		TEXT("Startech.Combat.Report: the combat log's session summary per enemy type and tier (time to kill, damage taken, profit, credits per minute)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PrintReport));

	static FAutoConsoleCommand TableCommand(TEXT("Startech.Combat.TTKTable"),
		TEXT("Startech.Combat.TTKTable: prints time to kill, health lost and ammo cost per enemy and tier with the expected gear ('*' = outside its target)."),
		FConsoleCommandDelegate::CreateStatic(&PrintTable));
}

#endif
