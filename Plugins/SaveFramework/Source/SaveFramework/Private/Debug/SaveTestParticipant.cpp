#include "Debug/SaveTestParticipant.h"

#include "SaveFrameworkLog.h"
#include "SaveSerialization.h"

const FName USaveTestParticipant::ParticipantId(TEXT("SaveFramework.Test"));

FName USaveTestParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 USaveTestParticipant::GetSaveVersion() const
{
	return bWriteLegacyV1 ? 1 : 2;
}

void USaveTestParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	if (bWriteLegacyV1)
	{
		FSaveTestDataV1 Legacy;
		Legacy.Counter = Data.Counter;
		Legacy.Note = Data.Note;
		Legacy.Label = Data.Label;
		Legacy.Asset = Data.Asset;
		Legacy.Nested = Data.Nested;
		FSaveSerialization::StructToBytes(Legacy, OutData);
		return;
	}
	FSaveSerialization::StructToBytes(Data, OutData);
}

void USaveTestParticipant::ReadSaveData(const TArray<uint8>& InData, const int32 SavedVersion)
{
	FSaveTestDataV2 Loaded;
	if (!FSaveSerialization::BytesToStruct(InData, Loaded))
	{
		UE_LOG(LogSaveFramework, Error, TEXT("[SaveTest] Record could not be read; starting from defaults."));
		OnNewGame();
		return;
	}
	if (SavedVersion < 2)
	{
		// Version 1 had no AddedField: it keeps its default (42). A real migration would convert values here.
		UE_LOG(LogSaveFramework, Display, TEXT("[SaveTest] Converted version %d data; AddedField defaulted to %d."), SavedVersion, Loaded.AddedField);
	}
	Data = Loaded;
	LogData(TEXT("Loaded"));
}

void USaveTestParticipant::OnNewGame()
{
	Data = FSaveTestDataV2();
	LogData(TEXT("New game"));
}

void USaveTestParticipant::LogData(const TCHAR* const Context) const
{
	UE_LOG(LogSaveFramework, Display, TEXT("[SaveTest] %s: Counter=%d Note=\"%s\" Label=%s Asset=%s Nested=%d AddedField=%d"),
		Context, Data.Counter, *Data.Note, *Data.Label.ToString(), *Data.Asset.ToString(), Data.Nested.NestedValue, Data.AddedField);
}
