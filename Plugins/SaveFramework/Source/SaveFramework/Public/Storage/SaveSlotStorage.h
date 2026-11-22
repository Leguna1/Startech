#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.h"

class USaveFrameworkSaveGame;

/**
 * Every file operation on slots: reading with integrity checks, falling back to the backup, writing (sync or async)
 * after backing up, deleting. Stateless; works on slot names so tests can use their own files.
 * Files go through the platform save system (UGameplayStatics), user index 0.
 */
struct SAVEFRAMEWORK_API FSaveSlotStorage
{
	/** The backup file name of SlotName: "<SlotName>_Backup". */
	static FString GetBackupSlotName(const FString& SlotName);

	/** True when the main file or the backup of SlotName exists. */
	static bool DoesSlotExist(const FString& SlotName);

	/** The save game in file FileName, or null (with a warning) when it is missing, unreadable or not intact. */
	static USaveFrameworkSaveGame* LoadIntact(const FString& FileName);

	/**
	 * Reads SlotName, falling back to its backup when the main file is missing or corrupt.
	 * OutSaveGame is set for Loaded and RestoredFromBackup, null otherwise.
	 */
	static ESaveSlotLoadResult LoadWithFallback(const FString& SlotName, USaveFrameworkSaveGame*& OutSaveGame);

	/** Copies the main file of SlotName to its backup, only when the main file is intact. True when copied. */
	static bool BackupExisting(const FString& SlotName);

	/** Backs up (when bKeepBackup is set), then writes SaveGame to SlotName synchronously. True on success. */
	static bool SaveSync(USaveFrameworkSaveGame& SaveGame, const FString& SlotName);

	/**
	 * Backs up (when bKeepBackup is set), serializes SaveGame now, and writes it on a worker thread.
	 * OnCompleted runs on the game thread with the result.
	 */
	static void SaveAsync(USaveFrameworkSaveGame& SaveGame, const FString& SlotName, TFunction<void(bool bSuccess)> OnCompleted);

	/** Deletes the main file and the backup of SlotName. True when nothing of it remains. */
	static bool DeleteSlot(const FString& SlotName);

	/** Size in bytes of file FileName, or 0 when it does not exist. */
	static int32 GetFileSize(const FString& FileName);

	/** Truncates file FileName to half its size, to test the backup. True when the file existed. */
	static bool TruncateForTest(const FString& FileName);

	/** The user index passed to the platform save system. */
	static constexpr int32 UserIndex = 0;
};
