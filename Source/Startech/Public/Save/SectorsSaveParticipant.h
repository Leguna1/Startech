#pragma once

#include "CoreMinimal.h"
#include "SaveParticipant.h"
#include "UObject/Object.h"
#include "SectorsSaveParticipant.generated.h"

class USectorDirectorSubsystem;

/**
 * Startech.Sectors: ProcSector's FSectorStateStore (remembered loot and stations consumed, asteroids mined), copied
 * into FStartechSectorsSave because ProcSector's FSectorState has no SaveGame flags. Created and registered by
 * UStartechSectorSaveSubsystem as the game world starts, before any sector is requested.
 */
UCLASS()
class STARTECH_API USectorsSaveParticipant : public UObject, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Sectors". */
	static const FName ParticipantId;

	/** Remembers the director whose state store this participant saves. */
	void Init(USectorDirectorSubsystem& InDirector);

	/** "Startech.Sectors". */
	virtual FName GetSaveParticipantId() const override;

	/** 1. */
	virtual int32 GetSaveVersion() const override;

	/** Writes every sector's remembered changes. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Replaces the director's state store (FSectorStateStore::SetAllStates). Takes effect at the next RequestSector. */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Forgets every sector's changes. */
	virtual void OnNewGame() override;

private:
	/** The world's sector director. */
	TWeakObjectPtr<USectorDirectorSubsystem> Director;
};
