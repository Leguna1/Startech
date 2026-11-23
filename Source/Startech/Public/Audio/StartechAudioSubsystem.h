#pragma once

#include "CoreMinimal.h"
#include "Audio/StartechAudioTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StartechAudioSubsystem.generated.h"

class USoundClass;
class USoundMix;
class UWorld;

/**
 * The sound groups' volumes and the mixes (UStartechSettings::Audio).
 * - Volumes go through SMix_Volumes: one adjuster per group, Master's applying to its children, so a group is heard at
 *   Master × its own volume. The mix is (re)pushed into every game world as its actors initialise, so it holds from
 *   the main menu's first sound onwards. Starts at the saved volumes (UStartechUserSettings, per machine), or the
 *   defaults; the Settings page changes them live and saves them when it closes.
 * - The station hub pushes SMix_Hub (SFX and its children fade to silent; music plays on) while it is open.
 * - The player's warp start plays WarpSound.
 * Effects in the world pause with the game by themselves; only the music is a UI sound.
 */
UCLASS()
class STARTECH_API UStartechAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The game instance's audio subsystem, or null. */
	static UStartechAudioSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Sets every group's volume, applied at once (FadeSeconds 0) or over FadeSeconds. */
	void SetVolumes(const FStartechVolumes& InVolumes, float FadeSeconds = 0.f);

	const FStartechVolumes& GetVolumes() const { return Volumes; }

	/** Writes the current volumes to the machine's user settings (GameUserSettings.ini). */
	void SaveVolumes() const;

	/** The volumes saved on this machine (UStartechUserSettings), or the defaults. What the game starts with. */
	static FStartechVolumes LoadSavedVolumes();

	/** Plays the slider's sample once as a UI sound (heard while paused), in its group: Weapons and Explosions only. */
	void PlayPreview(EStartechVolume Volume) const;

	/** Pushes SMix_Hub while the station hub is open, pops it when it closes. */
	void SetHubOpen(bool bOpen);

	/** True while SMix_Hub is pushed. */
	bool IsHubMixActive() const { return bHubMixPushed; }

	/** Plays UStartechSettings::Audio.WarpSound, non-spatial. */
	void PlayWarpSound(const UObject* WorldContext) const;

	/** The volume each group's adjuster gets from Volumes (Master's applies to its children). */
	static float GetAdjusterVolume(const FStartechVolumes& InVolumes, const USoundClass& Class);

private:
	/** Sets the adjusters from Volumes and pushes SMix_Volumes into World's audio device (popping it first, so it is never stacked). */
	void ApplyToWorld(UWorld& World, float FadeSeconds);

	/** Binds every game world of this game instance as its actors initialise. */
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	/** The game instance's world, or null. */
	UWorld* GetPlayWorld() const;

	FStartechVolumes Volumes;
	bool bHubMixPushed = false;
	FDelegateHandle WorldInitHandle;
};
