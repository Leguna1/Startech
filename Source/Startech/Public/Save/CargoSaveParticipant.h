#pragma once

#include "CoreMinimal.h"
#include "Save/ShipSaveParticipantBase.h"
#include "SaveParticipant.h"
#include "CargoSaveParticipant.generated.h"

/** Startech.Cargo: the cargo hold's minerals by tag (FStartechCargoSave). The capacity comes from the ship. */
UCLASS()
class STARTECH_API UCargoSaveParticipant : public UShipSaveParticipantBase, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Cargo". */
	static const FName ParticipantId;

	/** "Startech.Cargo". */
	virtual FName GetSaveParticipantId() const override;

	/** 1. */
	virtual int32 GetSaveVersion() const override;

	/** Writes every cargo stack by its ItemTag. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Replaces the cargo (UInventoryComponent::RestoreItems). Quests count nothing for it. */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Empties the cargo hold. */
	virtual void OnNewGame() override;
};
