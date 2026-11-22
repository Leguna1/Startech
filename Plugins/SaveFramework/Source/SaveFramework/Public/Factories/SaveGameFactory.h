#pragma once

#include "CoreMinimal.h"
#include "SaveTypes.h"

class ISaveParticipant;
class USaveFrameworkSaveGame;

/** Creates every save object the framework writes: participant records, slot headers and sealed save games. */
struct SAVEFRAMEWORK_API FSaveGameFactory
{
	/** A record holding Participant's current data, id and version. */
	static FSaveRecord MakeRecord(const ISaveParticipant& Participant);

	/** A header for SlotIndex stamped with the current time, framework version and project version. */
	static FSaveSlotHeader MakeHeader(int32 SlotIndex, const FString& DisplayName, double PlayTimeSeconds);

	/** A new save game (in the transient package) holding Header and Records, sealed with its checksum. */
	static USaveFrameworkSaveGame* MakeSaveGame(const FSaveSlotHeader& Header, const TArray<FSaveRecord>& Records);
};
