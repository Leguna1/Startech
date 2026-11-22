#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SaveFrameworkSettings.generated.h"

/** Save Framework settings (Project Settings > Plugins > Save Framework), saved in DefaultGame.ini. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Save Framework"))
class SAVEFRAMEWORK_API USaveFrameworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Places the settings under Plugins. */
	USaveFrameworkSettings();

	/** The settings' class default object. */
	static const USaveFrameworkSettings& Get();

	/** How many save slots the game offers. Slot indices run from 0 to NumSlots - 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Slots", meta = (ClampMin = "1"))
	int32 NumSlots = 3;

	/** Prefix of each slot's file name: slot 0 is "<Prefix>0", its backup "<Prefix>0_Backup". */
	UPROPERTY(Config, EditAnywhere, Category = "Slots")
	FString SlotNamePrefix = TEXT("Slot");

	/** RequestSave calls within this many seconds of the first one are merged into a single save. */
	UPROPERTY(Config, EditAnywhere, Category = "Saving", meta = (ClampMin = "0", Units = "s"))
	float AutosaveDebounceSeconds = 1.f;

	/** Before a slot is overwritten, copy its last intact file to "<Slot>_Backup", used when the main file is corrupt. */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	bool bKeepBackup = true;

	/** Save the active slot synchronously when the game instance shuts down. */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	bool bSaveOnShutdown = true;

	/** The project's own version, written into every slot header (for example "0.3.1"). */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	FString ProjectVersion = TEXT("1.0");

	/** Participant ids that must all have registered before OnAllParticipantsLoaded fires for the active slot. */
	UPROPERTY(Config, EditAnywhere, Category = "Participants")
	TArray<FName> ExpectedParticipantIds;

	/** The file name of slot SlotIndex: SlotNamePrefix followed by the index. */
	FString MakeSlotName(int32 SlotIndex) const;

	/** True when SlotIndex is between 0 and NumSlots - 1. */
	bool IsValidSlotIndex(int32 SlotIndex) const;
};
