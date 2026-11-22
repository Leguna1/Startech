#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Debug/SaveTestParticipant.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Runtime/SaveSubsystem.h"
#include "SaveFrameworkLog.h"
#include "SaveFrameworkSettings.h"
#include "Storage/SaveSlotStorage.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Console commands (Command pattern: one small function per name). Non-shipping builds only.
 *   Save.Now                     save the active slot immediately
 *   Save.List                    every slot with name, play time, date and size
 *   Save.Info                    the active slot, registered and pending participants
 *   Save.Delete <Index>          delete a slot and its backup
 *   Save.CorruptActiveForTest    truncate the active slot's file, to test the backup
 *   Save.New <Index> [Name]      start a new game in a slot
 *   Save.Load <Index>            load a slot
 *   Save.Close                   close the active slot without saving
 *   Save.Test.Register / Save.Test.Unregister
 *   Save.Test.Set <Counter> [Note]   change the test participant and request a save
 *   Save.Test.Print              log the test participant's data
 *   Save.Test.WriteV1 <0|1>      make the test participant write its old V1 layout
 */
namespace SaveConsole
{
	/** The test participant, kept alive here once created. */
	static TStrongObjectPtr<USaveTestParticipant> TestParticipant;

	/** The save subsystem of World's game instance, or null (with a warning) outside a running game. */
	static USaveSubsystem* GetSaves(const UWorld* const World)
	{
		const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
		USaveSubsystem* const Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
		if (!Saves)
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Save commands only work in a running game."));
		}
		return Saves;
	}

	/** Play time as h:mm:ss. */
	static FString FormatPlayTime(const double Seconds)
	{
		const int64 Total = FMath::FloorToInt64(Seconds);
		return FString::Printf(TEXT("%lld:%02lld:%02lld"), Total / 3600, (Total / 60) % 60, Total % 60);
	}

	/** The test participant, created on first use. */
	static USaveTestParticipant& GetTestParticipant()
	{
		if (!TestParticipant.IsValid())
		{
			TestParticipant.Reset(NewObject<USaveTestParticipant>());
		}
		return *TestParticipant.Get();
	}

	static void Now(const TArray<FString>& Args, UWorld* const World)
	{
		if (USaveSubsystem* const Saves = GetSaves(World))
		{
			Saves->SaveNow(TEXT("Console"));
		}
	}

	static void List(const TArray<FString>& Args, UWorld* const World)
	{
		USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves)
		{
			return;
		}
		Saves->RefreshSlotInfos();
		for (const FSaveSlotInfo& Info : Saves->GetSlotInfos())
		{
			const FString SlotName = Saves->GetSlotName(Info.Header.SlotIndex);
			if (!Info.bOccupied)
			{
				UE_LOG(LogSaveFramework, Display, TEXT("  [%d] %s: empty"), Info.Header.SlotIndex, *SlotName);
				continue;
			}
			UE_LOG(LogSaveFramework, Display, TEXT("  [%d] %s: \"%s\" played %s saved %s UTC, %d bytes (backup %d bytes), project %s%s%s"),
				Info.Header.SlotIndex, *SlotName, *Info.Header.DisplayName, *FormatPlayTime(Info.Header.PlayTimeSeconds),
				*Info.Header.SavedUtc.ToString(), FSaveSlotStorage::GetFileSize(SlotName),
				FSaveSlotStorage::GetFileSize(FSaveSlotStorage::GetBackupSlotName(SlotName)), *Info.Header.ProjectVersion,
				Info.bRestoredFromBackup ? TEXT(" (main file corrupt, from backup)") : TEXT(""),
				Info.Header.SlotIndex == Saves->GetActiveSlot() ? TEXT(" <- active") : TEXT(""));
		}
	}

	static void Info(const TArray<FString>& Args, UWorld* const World)
	{
		const USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves)
		{
			return;
		}
		if (Saves->HasActiveSlot())
		{
			const FSaveSlotHeader Header = Saves->GetActiveHeader();
			UE_LOG(LogSaveFramework, Display, TEXT("Active slot %d \"%s\", played %s%s%s"), Header.SlotIndex, *Header.DisplayName,
				*FormatPlayTime(Header.PlayTimeSeconds), Saves->WasActiveSlotRestoredFromBackup() ? TEXT(", restored from backup") : TEXT(""),
				Saves->IsSaveInFlight() ? TEXT(", saving") : TEXT(""));
		}
		else
		{
			UE_LOG(LogSaveFramework, Display, TEXT("No active slot."));
		}
		UE_LOG(LogSaveFramework, Display, TEXT("Registered: %s"), *FString::JoinBy(Saves->GetRegisteredParticipantIds(), TEXT(", "),
			[](const FName Id) { return Id.ToString(); }));
		UE_LOG(LogSaveFramework, Display, TEXT("Pending:    %s"), *FString::JoinBy(Saves->GetPendingParticipantIds(), TEXT(", "),
			[](const FName Id) { return Id.ToString(); }));
	}

	static void Delete(const TArray<FString>& Args, UWorld* const World)
	{
		USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves || Args.IsEmpty())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Usage: Save.Delete <Index>"));
			return;
		}
		Saves->DeleteSlot(FCString::Atoi(*Args[0]));
	}

	static void CorruptActive(const TArray<FString>& Args, UWorld* const World)
	{
		const USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves || !Saves->HasActiveSlot())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Save.CorruptActiveForTest: no active slot."));
			return;
		}
		const FString SlotName = Saves->GetSlotName(Saves->GetActiveSlot());
		if (FSaveSlotStorage::TruncateForTest(SlotName))
		{
			UE_LOG(LogSaveFramework, Display, TEXT("Truncated '%s' to %d bytes. Save.Load %d now reads the backup."), *SlotName,
				FSaveSlotStorage::GetFileSize(SlotName), Saves->GetActiveSlot());
		}
		else
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("'%s' has not been written yet."), *SlotName);
		}
	}

	static void New(const TArray<FString>& Args, UWorld* const World)
	{
		USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves || Args.IsEmpty())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Usage: Save.New <Index> [Name]"));
			return;
		}
		const FString Name = Args.IsValidIndex(1) ? FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" ")) : TEXT("Test");
		Saves->CreateNewGame(FCString::Atoi(*Args[0]), Name);
	}

	static void Load(const TArray<FString>& Args, UWorld* const World)
	{
		USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves || Args.IsEmpty())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Usage: Save.Load <Index>"));
			return;
		}
		FText Reason;
		if (!Saves->LoadSlot(FCString::Atoi(*Args[0]), Reason))
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Save.Load %s failed: %s"), *Args[0], *Reason.ToString());
		}
	}

	static void Close(const TArray<FString>& Args, UWorld* const World)
	{
		if (USaveSubsystem* const Saves = GetSaves(World))
		{
			Saves->CloseActiveSlot();
		}
	}

	static void TestRegister(const TArray<FString>& Args, UWorld* const World)
	{
		if (USaveSubsystem* const Saves = GetSaves(World))
		{
			Saves->RegisterParticipant(&GetTestParticipant());
		}
	}

	static void TestUnregister(const TArray<FString>& Args, UWorld* const World)
	{
		USaveSubsystem* const Saves = GetSaves(World);
		if (Saves && TestParticipant.IsValid())
		{
			Saves->UnregisterParticipant(TestParticipant.Get());
		}
	}

	static void TestSet(const TArray<FString>& Args, UWorld* const World)
	{
		USaveSubsystem* const Saves = GetSaves(World);
		if (!Saves || Args.IsEmpty())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Usage: Save.Test.Set <Counter> [Note]"));
			return;
		}
		USaveTestParticipant& Participant = GetTestParticipant();
		Participant.Data.Counter = FCString::Atoi(*Args[0]);
		Participant.Data.Note = Args.IsValidIndex(1) ? FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" ")) : FString();
		Participant.Data.Label = FName(*FString::Printf(TEXT("Label_%d"), Participant.Data.Counter));
		Participant.Data.Asset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Participant.Data.Nested.NestedValue = Participant.Data.Counter * 10;
		Participant.Data.AddedField = Participant.Data.Counter + 1000;
		Participant.LogData(TEXT("Set"));
		Saves->RequestSave(TEXT("Save.Test.Set"));
	}

	static void TestPrint(const TArray<FString>& Args, UWorld* const World)
	{
		GetTestParticipant().LogData(TEXT("Print"));
	}

	static void TestWriteV1(const TArray<FString>& Args, UWorld* const World)
	{
		USaveTestParticipant& Participant = GetTestParticipant();
		Participant.bWriteLegacyV1 = !Args.IsEmpty() && FCString::Atoi(*Args[0]) != 0;
		UE_LOG(LogSaveFramework, Display, TEXT("[SaveTest] Writes version %d from now on."), Participant.GetSaveVersion());
	}

	static FAutoConsoleCommandWithWorldAndArgs NowCommand(TEXT("Save.Now"),
		TEXT("Saves the active slot immediately."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Now));

	static FAutoConsoleCommandWithWorldAndArgs ListCommand(TEXT("Save.List"),
		TEXT("Logs every slot with its name, play time, date and size."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&List));

	static FAutoConsoleCommandWithWorldAndArgs InfoCommand(TEXT("Save.Info"),
		TEXT("Logs the active slot and the registered and pending participants."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Info));

	static FAutoConsoleCommandWithWorldAndArgs DeleteCommand(TEXT("Save.Delete"),
		TEXT("Save.Delete <Index>: deletes a slot and its backup."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Delete));

	static FAutoConsoleCommandWithWorldAndArgs CorruptCommand(TEXT("Save.CorruptActiveForTest"),
		TEXT("Truncates the active slot's file, to test the backup."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CorruptActive));

	static FAutoConsoleCommandWithWorldAndArgs NewCommand(TEXT("Save.New"),
		TEXT("Save.New <Index> [Name]: starts a new game in a slot."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&New));

	static FAutoConsoleCommandWithWorldAndArgs LoadCommand(TEXT("Save.Load"),
		TEXT("Save.Load <Index>: loads a slot."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Load));

	static FAutoConsoleCommandWithWorldAndArgs CloseCommand(TEXT("Save.Close"),
		TEXT("Closes the active slot without saving."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Close));

	static FAutoConsoleCommandWithWorldAndArgs TestRegisterCommand(TEXT("Save.Test.Register"),
		TEXT("Registers the test participant."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestRegister));

	static FAutoConsoleCommandWithWorldAndArgs TestUnregisterCommand(TEXT("Save.Test.Unregister"),
		TEXT("Unregisters the test participant."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestUnregister));

	static FAutoConsoleCommandWithWorldAndArgs TestSetCommand(TEXT("Save.Test.Set"),
		TEXT("Save.Test.Set <Counter> [Note]: changes the test participant's data and requests a save."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestSet));

	static FAutoConsoleCommandWithWorldAndArgs TestPrintCommand(TEXT("Save.Test.Print"),
		TEXT("Logs the test participant's data."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestPrint));

	static FAutoConsoleCommandWithWorldAndArgs TestWriteV1Command(TEXT("Save.Test.WriteV1"),
		TEXT("Save.Test.WriteV1 <0|1>: makes the test participant write its old version-1 layout."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TestWriteV1));
}

#endif
