#pragma once

#include "CoreMinimal.h"
#include "Audio/StartechAudioTypes.h"
#include "GameFramework/GameUserSettings.h"
#include "StartechUserSettings.generated.h"

/**
 * Per-machine settings, not per save slot: the engine's GameUserSettings (GameUserSettings.ini in the user's
 * Saved/Config), registered as GameUserSettingsClassName in DefaultEngine.ini. The engine loads it at start, before
 * any map, so UStartechAudioSubsystem applies the saved volumes before the main menu's music starts.
 * Holds the volume sliders; until the player changes them, the defaults in UStartechSettings::Audio.DefaultVolumes.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class STARTECH_API UStartechUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** The engine's user settings as this class, or null (GameUserSettingsClassName not set). */
	static UStartechUserSettings* Get();

	/** The saved volumes, or the defaults when none were saved. */
	FStartechVolumes GetVolumes() const;

	/** Remembers InVolumes; SaveSettings writes them. */
	void SetVolumes(const FStartechVolumes& InVolumes);

	/** Forgets the saved volumes: back to the defaults (tests restore the machine's settings with it). */
	void ClearSavedVolumes();

	/** True once volumes have been saved on this machine. */
	bool HasSavedVolumes() const { return bHasSavedVolumes; }

private:
	UPROPERTY(Config)
	FStartechVolumes Volumes;

	UPROPERTY(Config)
	bool bHasSavedVolumes = false;
};
