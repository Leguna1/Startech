#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "StartechMusicSubsystem.generated.h"

class UAudioComponent;
class UMusicPlaylist;
class USoundClass;
class UWorld;
struct FStreamableHandle;

/** What the music is doing. */
UENUM(BlueprintType)
enum class EMusicPhase : uint8
{
	Silent,
	FadingIn,
	Playing,
	FadingOut,
};

/** One sector's playlist, for tests that replace UStartechSettings::Audio's table. */
USTRUCT()
struct FMusicTableEntry
{
	GENERATED_BODY()

	FName SectorId;

	UPROPERTY()
	TObjectPtr<UMusicPlaylist> Playlist;
};

/**
 * The music player. It lives in the game instance, so its sound (a persistent, non-spatial UI sound that plays while
 * paused) survives map travel and a fade can run across the main menu → game switch.
 *
 * - The main menu calls PlayMenuMusic (AStartechMenuGameMode). Every game world's sector director is bound on its own:
 *   OnSectorTransitionStarted fades the music out unless the next sector shares the playlist, and OnSectorReady
 *   starts that sector's playlist. The player's warp start (AStartechShipPawn) begins the fade-out a little earlier.
 * - Leaving a map (UStartechGameFlowSubsystem) fades out.
 * - Changes are sequential: the old music fades out completely (MusicFadeOutSeconds), then the new fades in
 *   (MusicFadeInSeconds). The same playlist keeps playing, never restarted.
 * - A playlist plays a random take, loaded asynchronously, then another random take when it ends, never the same
 *   one twice in a row. Music continues through death; the respawn's sector change picks Terra's playlist.
 * The fades are driven here on the core ticker (real time, so also while paused), not by the audio component.
 */
UCLASS()
class STARTECH_API UStartechMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The game instance's music player, or null. */
	static UStartechMusicSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The main menu's playlist (UStartechSettings::Audio.MenuMusic), after any playing music fades out. */
	void PlayMenuMusic();

	/** SectorId's playlist, after any other music fades out; the same playlist just keeps playing. */
	void PlaySector(FName SectorId);

	/** A sector change has begun: fades out unless ToSectorId shares the playing playlist. */
	void PrepareSectorChange(FName ToSectorId);

	/** Fades out whatever plays, with nothing to follow (leaving a map). */
	void FadeOut();

	/** The playlist the sector plays, or null (silence). */
	UMusicPlaylist* FindSectorPlaylist(FName SectorId) const;

	/** The playlist playing or fading, or null. */
	const UMusicPlaylist* GetPlaylist() const { return Playlist; }

	/** The playlist that follows once the fade-out ends, or null. */
	const UMusicPlaylist* GetPendingPlaylist() const { return bHasPending ? PendingPlaylist.Get() : nullptr; }

	/** The take playing (or loading). */
	const FSoftObjectPath& GetTrack() const { return Track; }

	EMusicPhase GetPhase() const { return Phase; }

	/** The fade's volume, 0-1 (the groups' volumes apply on top). */
	float GetFadeVolume() const { return FadeVolume; }

	/** Takes started since the game began: a restart of the same music counts too. */
	int32 GetTrackStarts() const { return TrackStarts; }

	/** The sound playing, or null (no audio device, still loading, or silent). */
	UAudioComponent* GetAudioComponent() const { return AudioComponent; }

	/** Runs the fades for DeltaSeconds; the core ticker calls it every frame. Public for tests. */
	void Advance(float DeltaSeconds);

	/** The playing take ended: the next random take of the playlist. Bound to the sound's OnAudioFinished. */
	UFUNCTION()
	void HandleTrackFinished();

	/** Tests: replaces the settings' menu playlist and sector table (null Menu restores the settings). */
	void SetMusicTableForTest(UMusicPlaylist* Menu, const TArray<FMusicTableEntry>& Sectors);

private:
	/** Plays Requested in Class: kept if already playing, else after the current music fades out. */
	void Request(UMusicPlaylist* Requested, USoundClass* Class);

	/** Starts the pending playlist, fading in from silence. */
	void StartPending();

	/** Picks a random take (not the last) and starts loading it. */
	void StartRandomTrack();

	/** The picked take loaded: plays it if it is still the one wanted. */
	void HandleTrackLoaded(FSoftObjectPath LoadedTrack);

	/** Stops and drops the sound without OnAudioFinished picking another take. */
	void StopAudio();

	/** Binds the sector director of a game world in this game instance. */
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	UFUNCTION()
	void HandleSectorTransitionStarted(FName FromSectorId, FName ToSectorId);

	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** The game instance's world, or null. */
	UWorld* GetPlayWorld() const;

	UPROPERTY(Transient)
	TObjectPtr<UMusicPlaylist> Playlist;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> PlaylistClass;

	UPROPERTY(Transient)
	TObjectPtr<UMusicPlaylist> PendingPlaylist;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> PendingClass;

	/** True when PendingPlaylist (possibly null: silence) follows the fade-out. */
	bool bHasPending = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AudioComponent;

	/** Tests' replacement table (bUseTestTable). */
	UPROPERTY(Transient)
	TObjectPtr<UMusicPlaylist> TestMenuPlaylist;

	UPROPERTY(Transient)
	TArray<FMusicTableEntry> TestSectors;

	bool bUseTestTable = false;

	EMusicPhase Phase = EMusicPhase::Silent;
	float FadeVolume = 0.f;
	FSoftObjectPath Track;
	int32 TrackIndex = INDEX_NONE;
	int32 TrackStarts = 0;

	/** Set while StopAudio stops the sound on purpose. */
	bool bStoppingAudio = false;

	TSharedPtr<FStreamableHandle> LoadHandle;
	FTSTicker::FDelegateHandle Ticker;
	FDelegateHandle WorldInitHandle;
};
