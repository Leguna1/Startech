#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Debug/SaveTestParticipant.h"
#include "Debug/SaveTestTypes.h"
#include "Factories/SaveGameFactory.h"
#include "SaveFrameworkSaveGame.h"
#include "SaveSerialization.h"
#include "Storage/SaveSlotStorage.h"

namespace SaveFrameworkTests
{
	/** Slot name used by the storage test, so real slots are never touched. */
	static const TCHAR* const TestSlotName = TEXT("SaveFrameworkAutomationTest");

	/** Filled V2 test data. */
	static FSaveTestDataV2 MakeFilledData()
	{
		FSaveTestDataV2 Data;
		Data.Counter = 7;
		Data.Note = TEXT("Hello save");
		Data.Label = TEXT("Some.Label");
		Data.Asset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Data.Nested.NestedValue = 99;
		Data.AddedField = 123;
		Data.NotSaved = 555;
		return Data;
	}

	/** A sealed save game holding one test participant record with Counter. */
	static USaveFrameworkSaveGame* MakeSaveGame(const int32 Counter)
	{
		USaveTestParticipant* const Participant = NewObject<USaveTestParticipant>();
		Participant->Data.Counter = Counter;
		TArray<FSaveRecord> Records;
		Records.Add(FSaveGameFactory::MakeRecord(*Participant));
		return FSaveGameFactory::MakeSaveGame(FSaveGameFactory::MakeHeader(0, TEXT("Test"), 10.0 * Counter), Records);
	}

	/** Counter stored in SaveGame's test record, or INDEX_NONE. */
	static int32 ReadCounter(const USaveFrameworkSaveGame& SaveGame)
	{
		const FSaveRecord* const Record = SaveGame.FindRecord(USaveTestParticipant::ParticipantId);
		FSaveTestDataV2 Data;
		return Record && FSaveSerialization::BytesToStruct(Record->Data, Data) ? Data.Counter : INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveSerializationRoundTripTest, "SaveFramework.Serialization.RoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSaveSerializationRoundTripTest::RunTest(const FString& Parameters)
{
	const FSaveTestDataV2 Original = SaveFrameworkTests::MakeFilledData();
	TArray<uint8> Bytes;
	FSaveSerialization::StructToBytes(Original, Bytes);
	TestTrue(TEXT("Bytes written"), Bytes.Num() > 0);

	FSaveTestDataV2 Loaded;
	TestTrue(TEXT("Read back"), FSaveSerialization::BytesToStruct(Bytes, Loaded));
	TestEqual(TEXT("Counter"), Loaded.Counter, Original.Counter);
	TestEqual(TEXT("Note"), Loaded.Note, Original.Note);
	TestEqual(TEXT("Label (FName)"), Loaded.Label, Original.Label);
	TestEqual(TEXT("Asset (soft path)"), Loaded.Asset, Original.Asset);
	TestEqual(TEXT("Nested SaveGame member"), Loaded.Nested.NestedValue, Original.Nested.NestedValue);
	TestEqual(TEXT("AddedField"), Loaded.AddedField, Original.AddedField);
	TestEqual(TEXT("Non-SaveGame property not written"), Loaded.NotSaved, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveSerializationFieldChangesTest, "SaveFramework.Serialization.FieldChanges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSaveSerializationFieldChangesTest::RunTest(const FString& Parameters)
{
	// Old data (before AddedField existed) into the new struct.
	FSaveTestDataV1 Old;
	Old.Counter = 3;
	Old.Note = TEXT("old");
	Old.Label = TEXT("Old.Label");
	Old.Nested.NestedValue = 4;
	TArray<uint8> OldBytes;
	FSaveSerialization::StructToBytes(Old, OldBytes);

	FSaveTestDataV2 New;
	TestTrue(TEXT("V1 data reads into V2"), FSaveSerialization::BytesToStruct(OldBytes, New));
	TestEqual(TEXT("Counter kept"), New.Counter, 3);
	TestEqual(TEXT("Note kept"), New.Note, FString(TEXT("old")));
	TestEqual(TEXT("Label kept"), New.Label, FName(TEXT("Old.Label")));
	TestEqual(TEXT("Nested kept"), New.Nested.NestedValue, 4);
	TestEqual(TEXT("Added field keeps its default"), New.AddedField, 42);

	// New data (with a field the old struct lacks) into the old struct: the extra field is skipped.
	TArray<uint8> NewBytes;
	FSaveSerialization::StructToBytes(SaveFrameworkTests::MakeFilledData(), NewBytes);
	FSaveTestDataV1 Back;
	TestTrue(TEXT("V2 data reads into V1"), FSaveSerialization::BytesToStruct(NewBytes, Back));
	TestEqual(TEXT("Counter read"), Back.Counter, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveSerializationCorruptTest, "SaveFramework.Serialization.Corrupt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSaveSerializationCorruptTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Bytes;
	FSaveSerialization::StructToBytes(SaveFrameworkTests::MakeFilledData(), Bytes);

	FSaveTestDataV2 Target;
	Target.Counter = -1;

	TestFalse(TEXT("Empty data rejected"), FSaveSerialization::BytesToStruct(TArray<uint8>(), Target));

	TArray<uint8> Truncated = Bytes;
	Truncated.SetNum(Bytes.Num() / 2);
	TestFalse(TEXT("Truncated data rejected"), FSaveSerialization::BytesToStruct(Truncated, Target));

	TArray<uint8> Flipped = Bytes;
	Flipped[Flipped.Num() - 3] ^= 0xFF;
	TestFalse(TEXT("Altered data rejected"), FSaveSerialization::BytesToStruct(Flipped, Target));

	TArray<uint8> Garbage;
	for (int32 Index = 0; Index < 256; ++Index)
	{
		Garbage.Add(static_cast<uint8>((Index * 37 + 11) & 0xFF));
	}
	TestFalse(TEXT("Garbage rejected"), FSaveSerialization::BytesToStruct(Garbage, Target));

	TestEqual(TEXT("Target unchanged after failures"), Target.Counter, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveStorageBackupTest, "SaveFramework.Storage.Backup",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FSaveStorageBackupTest::RunTest(const FString& Parameters)
{
	const FString SlotName = SaveFrameworkTests::TestSlotName;
	FSaveSlotStorage::DeleteSlot(SlotName);

	USaveFrameworkSaveGame* Loaded = nullptr;
	TestTrue(TEXT("Starts empty"), FSaveSlotStorage::LoadWithFallback(SlotName, Loaded) == ESaveSlotLoadResult::Empty);

	// First save, then a second one: the first becomes the backup.
	TestTrue(TEXT("First save"), FSaveSlotStorage::SaveSync(*SaveFrameworkTests::MakeSaveGame(1), SlotName));
	TestTrue(TEXT("Second save"), FSaveSlotStorage::SaveSync(*SaveFrameworkTests::MakeSaveGame(2), SlotName));

	TestTrue(TEXT("Main loads"), FSaveSlotStorage::LoadWithFallback(SlotName, Loaded) == ESaveSlotLoadResult::Loaded);
	TestEqual(TEXT("Main holds the second save"), Loaded ? SaveFrameworkTests::ReadCounter(*Loaded) : INDEX_NONE, 2);

	// Corrupt the main file: the backup (first save) is used.
	TestTrue(TEXT("Truncated"), FSaveSlotStorage::TruncateForTest(SlotName));
	TestTrue(TEXT("Falls back to the backup"), FSaveSlotStorage::LoadWithFallback(SlotName, Loaded) == ESaveSlotLoadResult::RestoredFromBackup);
	TestEqual(TEXT("Backup holds the first save"), Loaded ? SaveFrameworkTests::ReadCounter(*Loaded) : INDEX_NONE, 1);

	// Saving over a corrupt main file must not replace the good backup.
	TestTrue(TEXT("Third save"), FSaveSlotStorage::SaveSync(*SaveFrameworkTests::MakeSaveGame(3), SlotName));
	USaveFrameworkSaveGame* const Backup = FSaveSlotStorage::LoadIntact(FSaveSlotStorage::GetBackupSlotName(SlotName));
	TestEqual(TEXT("Backup still holds the first save"), Backup ? SaveFrameworkTests::ReadCounter(*Backup) : INDEX_NONE, 1);

	// Both damaged: corrupt.
	FSaveSlotStorage::TruncateForTest(SlotName);
	FSaveSlotStorage::TruncateForTest(FSaveSlotStorage::GetBackupSlotName(SlotName));
	AddExpectedError(TEXT("both unusable"), EAutomationExpectedErrorFlags::Contains, 1);
	TestTrue(TEXT("Both damaged is corrupt"), FSaveSlotStorage::LoadWithFallback(SlotName, Loaded) == ESaveSlotLoadResult::Corrupt);

	TestTrue(TEXT("Deleted with its backup"), FSaveSlotStorage::DeleteSlot(SlotName));
	TestFalse(TEXT("Nothing left"), FSaveSlotStorage::DoesSlotExist(SlotName));
	return true;
}

#endif
