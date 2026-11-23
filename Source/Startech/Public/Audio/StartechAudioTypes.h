#pragma once

#include "CoreMinimal.h"
#include "StartechAudioTypes.generated.h"

class UMusicPlaylist;
class USoundBase;
class USoundClass;
class USoundMix;

STARTECH_API DECLARE_LOG_CATEGORY_EXTERN(LogStartechAudio, Log, All);

/** One volume slider. */
UENUM(BlueprintType)
enum class EStartechVolume : uint8
{
	Master,
	Music,
	MenuMusic,
	Weapons,
	Explosions,
};

/** The volume sliders, 0-1 each. The heard volume of a group is Master × its own. */
USTRUCT(BlueprintType)
struct STARTECH_API FStartechVolumes
{
	GENERATED_BODY()

	/** One slider's value. */
	float Get(EStartechVolume Volume) const;

	/** Sets one slider's value, clamped to 0-1. */
	void Set(EStartechVolume Volume, float Value);

	bool operator==(const FStartechVolumes& Other) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float Master = 1.f;

	/** Sector music (SC_Music). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float Music = 0.7f;

	/** The main menu's music (SC_MenuMusic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float MenuMusic = 0.7f;

	/** Lasers, the drainer beam, the charge-up (SC_Weapons). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float Weapons = 0.8f;

	/** Ship deaths (SC_Explosions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float Explosions = 0.9f;
};

/** One sector's music. */
USTRUCT(BlueprintType)
struct STARTECH_API FSectorMusic
{
	GENERATED_BODY()

	/** ProcSector's sector id ("Terra"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FName SectorId;

	/** Empty: silence in that sector. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<UMusicPlaylist> Playlist;
};

/** Everything audio in UStartechSettings::Audio (DefaultGame.ini). Assets are made by Tools/Audio/setup_audio.py. */
USTRUCT(BlueprintType)
struct STARTECH_API FStartechAudioSettings
{
	GENERATED_BODY()

	/** The main menu's playlist. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TSoftObjectPtr<UMusicPlaylist> MenuMusic;

	/** Each sector's playlist. A sector not listed is silent (with a warning logged once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<FSectorMusic> SectorMusic;

	/** Seconds the playing music takes to fade out (a warp, a respawn elsewhere, leaving a map). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0", Units = "s"))
	float MusicFadeOutSeconds = 2.f;

	/** Seconds new music takes to fade in, once the old has faded out completely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music", meta = (ClampMin = "0", Units = "s"))
	float MusicFadeInSeconds = 2.f;

	/** SC_Master: every other group is under it, and sounds with no class use it (AudioSettings in DefaultEngine.ini). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundClass> MasterClass;

	/** SC_Music: the music player plays sector playlists in it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundClass> MusicClass;

	/** SC_MenuMusic: the music player plays the menu playlist in it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundClass> MenuMusicClass;

	/** SC_SFX: every sound effect is in it or under it; the hub fades it out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundClass> SFXClass;

	/** SC_Weapons (under SFX). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundClass> WeaponsClass;

	/** SC_Explosions (under SFX). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundClass> ExplosionsClass;

	/** SMix_Volumes: one adjuster per group, set from the volumes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundMix> VolumeMix;

	/** SMix_Hub: pushed while the station hub is open (SFX to silent; music keeps playing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Groups")
	TSoftObjectPtr<USoundMix> HubMix;

	/** The volumes before the player changes any (and Reset to defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volumes")
	FStartechVolumes DefaultVolumes;

	/** The player's own laser start sound plays non-spatial (it would sound distant from the chase camera), at this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0", ClampMax = "2"))
	float PlayerLaserStartVolume = 0.7f;

	/** A ship's mounts firing the same start sound within this many seconds make one volley: one sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0", Units = "s"))
	float VolleyWindowSeconds = 0.05f;

	/** Played (non-spatial) when the player's warp starts. Empty: none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TSoftObjectPtr<USoundBase> WarpSound;

	/** The Settings page plays it once when the Weapons slider is let go (a UI sound, so heard while paused). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings page")
	TSoftObjectPtr<USoundBase> WeaponsPreviewSound;

	/** The Settings page plays it once when the Explosions slider is let go. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings page")
	TSoftObjectPtr<USoundBase> ExplosionsPreviewSound;
};
