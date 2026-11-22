#pragma once

#include "CoreMinimal.h"
#include "Save/ShipSaveParticipantBase.h"
#include "SaveParticipant.h"
#include "WalletSaveParticipant.generated.h"

/** Startech.Wallet: the credits (FStartechWalletSave). A new game starts with UStartechSettings::NewGameCredits. */
UCLASS()
class STARTECH_API UWalletSaveParticipant : public UShipSaveParticipantBase, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Wallet". */
	static const FName ParticipantId;

	/** "Startech.Wallet". */
	virtual FName GetSaveParticipantId() const override;

	/** 1. */
	virtual int32 GetSaveVersion() const override;

	/** Writes the inventory's credits. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Sets the inventory's credits (UInventoryComponent::RestoreCredits). */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Sets the credits to UStartechSettings::NewGameCredits. */
	virtual void OnNewGame() override;
};
