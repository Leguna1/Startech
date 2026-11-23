#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Audio/MusicPlaylist.h"
#include "Audio/StartechAudioSubsystem.h"
#include "Audio/StartechMusicSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "StartechShipPawn.h"
#include "WeaponEffectsComponent.h"

/**
 * Audio test aids (non-shipping):
 *   Startech.Test.Audio       logs [Audio]: the music (phase, playlist, take, fade volume, whether it is heard and in
 *                             which group), the volumes, the hub mix, and the player's laser start sounds so far
 *   Startech.Test.MusicNext   ends the playing take as if it had finished: the playlist's next random take
 * Engine aids that help with these: au.DumpActiveSounds (every sound playing, with its class), ListSoundClasses.
 */
namespace AudioConsole
{
	static const TCHAR* PhaseName(const EMusicPhase Phase)
	{
		switch (Phase)
		{
		case EMusicPhase::FadingIn: return TEXT("FadingIn");
		case EMusicPhase::Playing: return TEXT("Playing");
		case EMusicPhase::FadingOut: return TEXT("FadingOut");
		default: return TEXT("Silent");
		}
	}

	static void Audio(const TArray<FString>& Args, UWorld* const World)
	{
		if (const UStartechMusicSubsystem* const Music = UStartechMusicSubsystem::Get(World))
		{
			const UAudioComponent* const Component = Music->GetAudioComponent();
			UE_LOG(LogStartechAudio, Display, TEXT("[Audio] music %s playlist=%s take=%s fade=%.2f heard=%s class=%s starts=%d pending=%s"),
				PhaseName(Music->GetPhase()), Music->GetPlaylist() ? *Music->GetPlaylist()->GetName() : TEXT("-"),
				*Music->GetTrack().GetAssetName(), Music->GetFadeVolume(),
				Component && Component->IsPlaying() ? TEXT("yes") : TEXT("no"),
				Component && Component->SoundClassOverride ? *Component->SoundClassOverride->GetName() : TEXT("-"),
				Music->GetTrackStarts(), Music->GetPendingPlaylist() ? *Music->GetPendingPlaylist()->GetName() : TEXT("-"));
		}
		if (const UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(World))
		{
			const FStartechVolumes& Volumes = Audio->GetVolumes();
			UE_LOG(LogStartechAudio, Display, TEXT("[Audio] volumes master %.2f music %.2f menu %.2f weapons %.2f explosions %.2f; hub mix %s"),
				Volumes.Master, Volumes.Music, Volumes.MenuMusic, Volumes.Weapons, Volumes.Explosions,
				Audio->IsHubMixActive() ? TEXT("ON") : TEXT("off"));
		}
		const AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		if (const UWeaponEffectsComponent* const Effects = Ship ? Ship->FindComponentByClass<UWeaponEffectsComponent>() : nullptr)
		{
			UE_LOG(LogStartechAudio, Display, TEXT("[Audio] player laser start sounds played: %d"), Effects->GetStartSoundsPlayed());
		}
	}

	static void MusicNext(const TArray<FString>& Args, UWorld* const World)
	{
		if (UStartechMusicSubsystem* const Music = UStartechMusicSubsystem::Get(World))
		{
			Music->HandleTrackFinished();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs AudioCommand(TEXT("Startech.Test.Audio"),
		TEXT("Logs the music, the volumes, the hub mix and the player's laser start sounds."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Audio));

	static FAutoConsoleCommandWithWorldAndArgs MusicNextCommand(TEXT("Startech.Test.MusicNext"),
		TEXT("Ends the playing take as if it had finished: the playlist's next random take."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MusicNext));
}

#endif
