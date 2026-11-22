#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.generated.h"

/**
 * Metadata of one saved slot, shown in menus without applying the save.
 * Every field is SaveGame: USaveGame files are written with ArIsSaveGame, which skips anything else.
 */
USTRUCT(BlueprintType)
struct SAVEFRAMEWORK_API FSaveSlotHeader
{
	GENERATED_BODY()

	/** The slot this save belongs to (0 to NumSlots - 1). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 SlotIndex = INDEX_NONE;

	/** Name shown in menus. The project sets it, for example the pilot name. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FString DisplayName;

	/** Seconds played in this slot while the game was not paused. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	double PlayTimeSeconds = 0.0;

	/** When the slot was last written, in UTC. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FDateTime SavedUtc;

	/** The framework's file format version when the slot was written (USaveFrameworkSaveGame::CurrentFrameworkVersion). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 FrameworkVersion = 0;

	/** USaveFrameworkSettings::ProjectVersion when the slot was written. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FString ProjectVersion;
};

/** One participant's saved data. The framework stores the bytes and never interprets them. */
USTRUCT()
struct SAVEFRAMEWORK_API FSaveRecord
{
	GENERATED_BODY()

	/** The participant that wrote the record (ISaveParticipant::GetSaveParticipantId). */
	UPROPERTY(SaveGame)
	FName ParticipantId;

	/** The participant's data version when it wrote the record (ISaveParticipant::GetSaveVersion). */
	UPROPERTY(SaveGame)
	int32 Version = 0;

	/** The participant's opaque data, usually written by FSaveSerialization::StructToBytes. */
	UPROPERTY(SaveGame)
	TArray<uint8> Data;
};

/** A slot as a menu sees it. */
USTRUCT(BlueprintType)
struct SAVEFRAMEWORK_API FSaveSlotInfo
{
	GENERATED_BODY()

	/** The slot's header. Only SlotIndex is meaningful when bOccupied is false. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FSaveSlotHeader Header;

	/** True when the slot holds a readable save (its main file or its backup). */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bOccupied = false;

	/** True when the main file was corrupt and this information comes from the backup. */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bRestoredFromBackup = false;
};

/** Outcome of reading a slot from disk (FSaveSlotStorage::LoadWithFallback). */
enum class ESaveSlotLoadResult : uint8
{
	/** The main file was intact and is returned. */
	Loaded,
	/** The main file was missing or corrupt; the intact backup is returned. */
	RestoredFromBackup,
	/** Neither the main file nor a backup exists. */
	Empty,
	/** A file exists, but neither it nor the backup is intact. */
	Corrupt
};
