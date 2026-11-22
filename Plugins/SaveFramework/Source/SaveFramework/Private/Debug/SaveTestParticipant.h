#pragma once

#include "CoreMinimal.h"
#include "Debug/SaveTestTypes.h"
#include "SaveParticipant.h"
#include "UObject/Object.h"
#include "SaveTestParticipant.generated.h"

/**
 * Test only: a participant driven from the console (Save.Test.*), reachable only in non-shipping builds.
 * Its data is FSaveTestDataV2 (version 2). With bWriteLegacyV1 it writes FSaveTestDataV1 as version 1, which shows
 * that data from before a field was added still loads.
 */
UCLASS(Transient)
class USaveTestParticipant : public UObject, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** The record id: "SaveFramework.Test". */
	static const FName ParticipantId;

	/** The current data. */
	FSaveTestDataV2 Data;

	/** When true, writes the old V1 layout as version 1. */
	bool bWriteLegacyV1 = false;

	/** "SaveFramework.Test". */
	virtual FName GetSaveParticipantId() const override;

	/** 2, or 1 while bWriteLegacyV1 is set. */
	virtual int32 GetSaveVersion() const override;

	/** Writes Data (or its V1 subset). */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Reads into a default FSaveTestDataV2 and logs the outcome, including the version conversion. */
	virtual void ReadSaveData(const TArray<uint8>& InData, int32 SavedVersion) override;

	/** Resets Data and logs it. */
	virtual void OnNewGame() override;

	/** Logs Data. */
	void LogData(const TCHAR* Context) const;
};
