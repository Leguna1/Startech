#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveTypes.h"
#include "SaveFrameworkSaveGame.generated.h"

/**
 * The object written to a slot file: the header, one record per participant, and a checksum.
 * Created by FSaveGameFactory, read and written by FSaveSlotStorage.
 */
UCLASS()
class SAVEFRAMEWORK_API USaveFrameworkSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** The file format version written into new headers. Raise it only when the layout of this class changes. */
	static constexpr int32 CurrentFrameworkVersion = 1;

	/** The slot's metadata. */
	UPROPERTY(SaveGame)
	FSaveSlotHeader Header;

	/** One record per participant, in no particular order. */
	UPROPERTY(SaveGame)
	TArray<FSaveRecord> Records;

	/**
	 * Checksum of Header.SlotIndex and every record (Seal). Declared last so it is serialized last: a truncated
	 * file loses it, and IsIntact then fails.
	 */
	UPROPERTY(SaveGame)
	uint32 Checksum = 0;

	/** The checksum of the current contents. Never 0, so a missing checksum never matches. */
	uint32 ComputeChecksum() const;

	/** Stores ComputeChecksum in Checksum. Call after the last change, before writing. */
	void Seal();

	/** True when the framework version is known and Checksum matches the contents. */
	bool IsIntact() const;

	/** The record of ParticipantId, or null. */
	const FSaveRecord* FindRecord(FName ParticipantId) const;
};
