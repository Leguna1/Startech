#pragma once

#include "CoreMinimal.h"
#include "Save/ShipSaveParticipantBase.h"
#include "SaveParticipant.h"
#include "ArsenalSaveParticipant.generated.h"

struct FStartechArsenalSave;

/**
 * Startech.Arsenal: the weapon on each mount, the spare weapons, every ammo count and the selected ammo
 * (FStartechArsenalSave), all by ItemTag. A new game uses UStartechSettings' NewGameMountWeapons, NewGameAmmo and
 * NewGameSelectedAmmo, with no spares.
 */
UCLASS()
class STARTECH_API UArsenalSaveParticipant : public UShipSaveParticipantBase, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Arsenal". */
	static const FName ParticipantId;

	/** "Startech.Arsenal". */
	virtual FName GetSaveParticipantId() const override;

	/** 2: one selected ammo per weapon group. Version 1 records (one selection) still load. */
	virtual int32 GetSaveVersion() const override;

	/** Writes every mount's weapon, the spares, the ammo and the selection. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Restores the mounts (FShipFitting::RestoreWeapon), spares, ammo, then the selection. */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Applies the new-game weapons and ammo from UStartechSettings. */
	virtual void OnNewGame() override;

private:
	/** Puts Save on the ship. Shared by ReadSaveData and OnNewGame. */
	void Apply(const FStartechArsenalSave& Save) const;
};
