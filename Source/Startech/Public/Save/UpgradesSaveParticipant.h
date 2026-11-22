#pragma once

#include "CoreMinimal.h"
#include "SaveParticipant.h"
#include "UObject/Object.h"
#include "UpgradesSaveParticipant.generated.h"

class UShipUpgradeSubsystem;

/**
 * Startech.Upgrades: every upgrade track's level (FStartechUpgradesSave). Created and registered by
 * UStartechSaveSubsystem when the game instance starts, like Startech.Profile. Saves made before upgrades have no
 * record, so they receive OnNewGame: every track at 0.
 */
UCLASS()
class STARTECH_API UUpgradesSaveParticipant : public UObject, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Upgrades". */
	static const FName ParticipantId;

	/** Remembers the subsystem this participant saves. */
	void Init(UShipUpgradeSubsystem& InUpgrades);

	/** "Startech.Upgrades". */
	virtual FName GetSaveParticipantId() const override;

	/** 1. */
	virtual int32 GetSaveVersion() const override;

	/** Writes every track above level 0. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Restores the levels (UShipUpgradeSubsystem::RestoreLevels). */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Every track to 0. */
	virtual void OnNewGame() override;

private:
	/** The upgrade subsystem. */
	TWeakObjectPtr<UShipUpgradeSubsystem> Upgrades;
};
