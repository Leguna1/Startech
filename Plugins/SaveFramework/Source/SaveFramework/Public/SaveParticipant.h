#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveParticipant.generated.h"

/** UObject side of ISaveParticipant. Native only: Blueprints cannot implement it. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USaveParticipant : public UInterface
{
	GENERATED_BODY()
};

/**
 * A system that saves and loads its own data. Implement it on any UObject (a component, a subsystem, an actor) and
 * call USaveSubsystem::RegisterParticipant when the object is ready, UnregisterParticipant before it goes away.
 *
 * On registration the object receives exactly one of ReadSaveData (the active slot holds its record) or OnNewGame.
 * Participants must not depend on each other's load order; one that must waits for OnAllParticipantsLoaded.
 */
class SAVEFRAMEWORK_API ISaveParticipant
{
	GENERATED_BODY()

public:
	/** Unique, stable id of this participant's record, for example "Player.Wallet". Never change it once shipped. */
	virtual FName GetSaveParticipantId() const = 0;

	/** The current version of this participant's data. Raise it when old data needs converting in ReadSaveData. */
	virtual int32 GetSaveVersion() const = 0;

	/** Writes this participant's data, usually with FSaveSerialization::StructToBytes. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const = 0;

	/** Restores this participant's data. SavedVersion is the GetSaveVersion that wrote it; convert when it is older. */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) = 0;

	/** Resets to new-game defaults. Called when the active slot holds no record for this participant. */
	virtual void OnNewGame() = 0;
};
