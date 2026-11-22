#pragma once

#include "CoreMinimal.h"
#include "SaveParticipant.h"
#include "UObject/Object.h"
#include "ProfileSaveParticipant.generated.h"

class UPlayerProfileSubsystem;

/**
 * Startech.Profile: the callsign and the highest sector tier visited (FStartechProfileSave). The ship id is derived from it and not stored.
 * Created and registered by UStartechSaveSubsystem when the game instance starts, so it receives its data in the menu.
 */
UCLASS()
class STARTECH_API UProfileSaveParticipant : public UObject, public ISaveParticipant
{
	GENERATED_BODY()

public:
	/** "Startech.Profile". */
	static const FName ParticipantId;

	/** Remembers the profile this participant saves. */
	void Init(UPlayerProfileSubsystem& InProfile);

	/** "Startech.Profile". */
	virtual FName GetSaveParticipantId() const override;

	/** 2 (version 1 had no highest tier visited; it loads as 1). */
	virtual int32 GetSaveVersion() const override;

	/** Writes the callsign and the highest tier visited. */
	virtual void WriteSaveData(TArray<uint8>& OutData) const override;

	/** Sets the callsign and the highest tier visited. */
	virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override;

	/** Restores the default callsign and tier 1; the new-game screen sets the chosen callsign right after. */
	virtual void OnNewGame() override;

private:
	/** The profile subsystem. */
	TWeakObjectPtr<UPlayerProfileSubsystem> Profile;
};
