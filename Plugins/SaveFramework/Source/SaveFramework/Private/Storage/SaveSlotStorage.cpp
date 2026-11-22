#include "Storage/SaveSlotStorage.h"

#include "Kismet/GameplayStatics.h"
#include "SaveFrameworkLog.h"
#include "SaveFrameworkSaveGame.h"
#include "SaveFrameworkSettings.h"

namespace SaveSlotStoragePrivate
{
	/** The intact save game held in Bytes, or null. FileName is only for the log. */
	static USaveFrameworkSaveGame* ParseIntact(const TArray<uint8>& Bytes, const FString& FileName)
	{
		if (Bytes.IsEmpty())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("'%s' is empty."), *FileName);
			return nullptr;
		}
		USaveFrameworkSaveGame* const SaveGame = Cast<USaveFrameworkSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!SaveGame)
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("'%s' is not a readable SaveFramework file."), *FileName);
			return nullptr;
		}
		if (!SaveGame->IsIntact())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("'%s' is corrupt (checksum mismatch)."), *FileName);
			return nullptr;
		}
		return SaveGame;
	}
}

FString FSaveSlotStorage::GetBackupSlotName(const FString& SlotName)
{
	return SlotName + TEXT("_Backup");
}

bool FSaveSlotStorage::DoesSlotExist(const FString& SlotName)
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)
		|| UGameplayStatics::DoesSaveGameExist(GetBackupSlotName(SlotName), UserIndex);
}

USaveFrameworkSaveGame* FSaveSlotStorage::LoadIntact(const FString& FileName)
{
	TArray<uint8> Bytes;
	if (!UGameplayStatics::DoesSaveGameExist(FileName, UserIndex) || !UGameplayStatics::LoadDataFromSlot(Bytes, FileName, UserIndex))
	{
		return nullptr;
	}
	return SaveSlotStoragePrivate::ParseIntact(Bytes, FileName);
}

ESaveSlotLoadResult FSaveSlotStorage::LoadWithFallback(const FString& SlotName, USaveFrameworkSaveGame*& OutSaveGame)
{
	OutSaveGame = LoadIntact(SlotName);
	if (OutSaveGame)
	{
		return ESaveSlotLoadResult::Loaded;
	}

	const bool bMainExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
	const FString BackupName = GetBackupSlotName(SlotName);
	OutSaveGame = LoadIntact(BackupName);
	if (OutSaveGame)
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("'%s' is %s; restored from '%s' (saved %s)."), *SlotName,
			bMainExists ? TEXT("corrupt") : TEXT("missing"), *BackupName, *OutSaveGame->Header.SavedUtc.ToString());
		return ESaveSlotLoadResult::RestoredFromBackup;
	}

	if (!bMainExists && !UGameplayStatics::DoesSaveGameExist(BackupName, UserIndex))
	{
		return ESaveSlotLoadResult::Empty;
	}
	UE_LOG(LogSaveFramework, Error, TEXT("'%s' and its backup are both unusable."), *SlotName);
	return ESaveSlotLoadResult::Corrupt;
}

bool FSaveSlotStorage::BackupExisting(const FString& SlotName)
{
	TArray<uint8> Bytes;
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex) || !UGameplayStatics::LoadDataFromSlot(Bytes, SlotName, UserIndex))
	{
		return false;
	}
	// Never let a corrupt main file replace a good backup.
	if (!SaveSlotStoragePrivate::ParseIntact(Bytes, SlotName))
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("'%s' is not intact; its backup is kept as it is."), *SlotName);
		return false;
	}
	const FString BackupName = GetBackupSlotName(SlotName);
	if (!UGameplayStatics::SaveDataToSlot(Bytes, BackupName, UserIndex))
	{
		UE_LOG(LogSaveFramework, Error, TEXT("Could not write the backup '%s'."), *BackupName);
		return false;
	}
	return true;
}

bool FSaveSlotStorage::SaveSync(USaveFrameworkSaveGame& SaveGame, const FString& SlotName)
{
	if (USaveFrameworkSettings::Get().bKeepBackup)
	{
		BackupExisting(SlotName);
	}
	const bool bSuccess = UGameplayStatics::SaveGameToSlot(&SaveGame, SlotName, UserIndex);
	if (!bSuccess)
	{
		UE_LOG(LogSaveFramework, Error, TEXT("Could not write '%s'."), *SlotName);
	}
	return bSuccess;
}

void FSaveSlotStorage::SaveAsync(USaveFrameworkSaveGame& SaveGame, const FString& SlotName, TFunction<void(bool bSuccess)> OnCompleted)
{
	if (USaveFrameworkSettings::Get().bKeepBackup)
	{
		BackupExisting(SlotName);
	}
	UGameplayStatics::AsyncSaveGameToSlot(&SaveGame, SlotName, UserIndex, FAsyncSaveGameToSlotDelegate::CreateLambda(
		[Callback = MoveTemp(OnCompleted)](const FString& WrittenSlot, const int32 WrittenUserIndex, const bool bSuccess)
		{
			if (!bSuccess)
			{
				UE_LOG(LogSaveFramework, Error, TEXT("Could not write '%s'."), *WrittenSlot);
			}
			if (Callback)
			{
				Callback(bSuccess);
			}
		}));
}

bool FSaveSlotStorage::DeleteSlot(const FString& SlotName)
{
	const FString BackupName = GetBackupSlotName(SlotName);
	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
	}
	if (UGameplayStatics::DoesSaveGameExist(BackupName, UserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(BackupName, UserIndex);
	}
	return !DoesSlotExist(SlotName);
}

int32 FSaveSlotStorage::GetFileSize(const FString& FileName)
{
	TArray<uint8> Bytes;
	if (UGameplayStatics::DoesSaveGameExist(FileName, UserIndex) && UGameplayStatics::LoadDataFromSlot(Bytes, FileName, UserIndex))
	{
		return Bytes.Num();
	}
	return 0;
}

bool FSaveSlotStorage::TruncateForTest(const FString& FileName)
{
	TArray<uint8> Bytes;
	if (!UGameplayStatics::DoesSaveGameExist(FileName, UserIndex) || !UGameplayStatics::LoadDataFromSlot(Bytes, FileName, UserIndex))
	{
		return false;
	}
	Bytes.SetNum(Bytes.Num() / 2);
	return UGameplayStatics::SaveDataToSlot(Bytes, FileName, UserIndex);
}
