#include "Audio/StartechMusicSubsystem.h"

#include "Audio/MusicPlaylist.h"
#include "Audio/StartechAudioTypes.h"
#include "Components/AudioComponent.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "StartechSettings.h"

namespace StartechMusic
{
	/** A long hitch (a map load) must not finish a fade in one step. */
	constexpr float MaxStepSeconds = 0.1f;

	/** Never fully 0: a component at volume 0 may be virtualised and restart its take. */
	constexpr float MinComponentVolume = 0.001f;
}

UStartechMusicSubsystem* UStartechMusicSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UStartechMusicSubsystem>() : nullptr;
}

void UStartechMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UStartechMusicSubsystem::HandleWorldActorsInitialized);
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](const float DeltaSeconds)
	{
		Advance(FMath::Min(DeltaSeconds, StartechMusic::MaxStepSeconds));
		return true;
	}));
}

void UStartechMusicSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
	StopAudio();
	Super::Deinitialize();
}

UWorld* UStartechMusicSubsystem::GetPlayWorld() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void UStartechMusicSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	UWorld* const World = Params.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	if (USectorDirectorSubsystem* const Director = World->GetSubsystem<USectorDirectorSubsystem>())
	{
		Director->OnSectorTransitionStarted.AddUniqueDynamic(this, &UStartechMusicSubsystem::HandleSectorTransitionStarted);
		Director->OnSectorReady.AddUniqueDynamic(this, &UStartechMusicSubsystem::HandleSectorReady);
	}
}

void UStartechMusicSubsystem::HandleSectorTransitionStarted(const FName FromSectorId, const FName ToSectorId)
{
	PrepareSectorChange(ToSectorId);
}

void UStartechMusicSubsystem::HandleSectorReady(const FName SectorId)
{
	PlaySector(SectorId);
}

UMusicPlaylist* UStartechMusicSubsystem::FindSectorPlaylist(const FName SectorId) const
{
	if (bUseTestTable)
	{
		const FMusicTableEntry* const Entry = TestSectors.FindByPredicate([SectorId](const FMusicTableEntry& Each) { return Each.SectorId == SectorId; });
		return Entry ? Entry->Playlist.Get() : nullptr;
	}

	const FSectorMusic* const Entry = UStartechSettings::Get().Audio.SectorMusic.FindByPredicate(
		[SectorId](const FSectorMusic& Each) { return Each.SectorId == SectorId; });
	if (!Entry)
	{
		static TArray<FName> Warned;
		if (!Warned.Contains(SectorId))
		{
			Warned.Add(SectorId);
			UE_LOG(LogStartechAudio, Warning, TEXT("[Music] Sector '%s' has no entry in UStartechSettings::Audio.SectorMusic: it is silent."), *SectorId.ToString());
		}
		return nullptr;
	}
	return Entry->Playlist.LoadSynchronous();
}

void UStartechMusicSubsystem::PlayMenuMusic()
{
	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	Request(bUseTestTable ? TestMenuPlaylist.Get() : Settings.MenuMusic.LoadSynchronous(), Settings.MenuMusicClass.LoadSynchronous());
}

void UStartechMusicSubsystem::PlaySector(const FName SectorId)
{
	Request(FindSectorPlaylist(SectorId), UStartechSettings::Get().Audio.MusicClass.LoadSynchronous());
}

void UStartechMusicSubsystem::PrepareSectorChange(const FName ToSectorId)
{
	if (Phase == EMusicPhase::Silent)
	{
		return;
	}
	if (FindSectorPlaylist(ToSectorId) == Playlist && Phase != EMusicPhase::FadingOut)
	{
		return;
	}
	// The next sector's playlist is requested when it is ready.
	bHasPending = false;
	Phase = EMusicPhase::FadingOut;
	UE_LOG(LogStartechAudio, Log, TEXT("[Music] fading out for '%s'."), *ToSectorId.ToString());
}

void UStartechMusicSubsystem::FadeOut()
{
	bHasPending = false;
	if (Phase != EMusicPhase::Silent)
	{
		Phase = EMusicPhase::FadingOut;
		UE_LOG(LogStartechAudio, Log, TEXT("[Music] fading out."));
	}
}

void UStartechMusicSubsystem::Request(UMusicPlaylist* const Requested, USoundClass* const Class)
{
	if (Requested && Requested == Playlist && Phase != EMusicPhase::Silent)
	{
		// The same music: keep it (and come back up if it had started fading).
		bHasPending = false;
		if (Phase == EMusicPhase::FadingOut)
		{
			Phase = EMusicPhase::FadingIn;
		}
		return;
	}

	PendingPlaylist = Requested;
	PendingClass = Class;
	bHasPending = true;
	if (Phase == EMusicPhase::Silent)
	{
		StartPending();
	}
	else
	{
		Phase = EMusicPhase::FadingOut;
	}
}

void UStartechMusicSubsystem::StartPending()
{
	bHasPending = false;
	StopAudio();
	Playlist = PendingPlaylist;
	PlaylistClass = PendingClass;
	PendingPlaylist = nullptr;
	PendingClass = nullptr;
	TrackIndex = INDEX_NONE;

	if (!Playlist || Playlist->Tracks.IsEmpty())
	{
		Playlist = nullptr;
		Phase = EMusicPhase::Silent;
		FadeVolume = 0.f;
		Track.Reset();
		return;
	}

	const float FadeIn = UStartechSettings::Get().Audio.MusicFadeInSeconds;
	Phase = FadeIn > 0.f ? EMusicPhase::FadingIn : EMusicPhase::Playing;
	FadeVolume = FadeIn > 0.f ? 0.f : 1.f;
	StartRandomTrack();
}

void UStartechMusicSubsystem::StartRandomTrack()
{
	const int32 Count = Playlist->Tracks.Num();
	int32 Next = FMath::RandRange(0, Count - 1);
	if (Count > 1 && Next == TrackIndex)
	{
		Next = (Next + 1 + FMath::RandRange(0, Count - 2)) % Count;
	}
	TrackIndex = Next;
	Track = Playlist->Tracks[Next].ToSoftObjectPath();
	++TrackStarts;
	StopAudio();
	UE_LOG(LogStartechAudio, Log, TEXT("[Music] %s: take %d/%d %s"), *Playlist->GetName(), Next + 1, Count, *Track.GetAssetName());

	if (Track.ResolveObject())
	{
		HandleTrackLoaded(Track);
		return;
	}
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Track,
		FStreamableDelegate::CreateWeakLambda(this, [this, Wanted = Track]() { HandleTrackLoaded(Wanted); }));
}

void UStartechMusicSubsystem::HandleTrackLoaded(const FSoftObjectPath LoadedTrack)
{
	if (LoadedTrack != Track || Phase == EMusicPhase::Silent)
	{
		return;
	}
	USoundBase* const Sound = Cast<USoundBase>(LoadedTrack.ResolveObject());
	UWorld* const World = GetPlayWorld();
	if (!Sound || !World)
	{
		return;
	}

	// Persistent (survives map travel and level flushes) and not auto-destroyed: it is reused only for this take.
	AudioComponent = UGameplayStatics::CreateSound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, true, false);
	if (!AudioComponent)
	{
		return;
	}
	AudioComponent->bIsUISound = true;
	AudioComponent->SoundClassOverride = PlaylistClass;
	AudioComponent->SetVolumeMultiplier(FMath::Max(FadeVolume, StartechMusic::MinComponentVolume));
	AudioComponent->OnAudioFinished.AddUniqueDynamic(this, &UStartechMusicSubsystem::HandleTrackFinished);
	AudioComponent->Play();
}

void UStartechMusicSubsystem::HandleTrackFinished()
{
	if (bStoppingAudio || !Playlist || Phase == EMusicPhase::Silent || Phase == EMusicPhase::FadingOut)
	{
		return;
	}
	StartRandomTrack();
}

void UStartechMusicSubsystem::StopAudio()
{
	LoadHandle.Reset();
	if (UAudioComponent* const Component = AudioComponent)
	{
		AudioComponent = nullptr;
		TGuardValue<bool> Stopping(bStoppingAudio, true);
		Component->OnAudioFinished.RemoveAll(this);
		Component->Stop();
		Component->DestroyComponent();
	}
}

void UStartechMusicSubsystem::Advance(const float DeltaSeconds)
{
	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	if (Phase == EMusicPhase::FadingIn)
	{
		FadeVolume = Settings.MusicFadeInSeconds > 0.f ? FadeVolume + DeltaSeconds / Settings.MusicFadeInSeconds : 1.f;
		if (FadeVolume >= 1.f)
		{
			FadeVolume = 1.f;
			Phase = EMusicPhase::Playing;
		}
	}
	else if (Phase == EMusicPhase::FadingOut)
	{
		FadeVolume = Settings.MusicFadeOutSeconds > 0.f ? FadeVolume - DeltaSeconds / Settings.MusicFadeOutSeconds : 0.f;
		if (FadeVolume <= 0.f)
		{
			FadeVolume = 0.f;
			StopAudio();
			Playlist = nullptr;
			PlaylistClass = nullptr;
			Track.Reset();
			Phase = EMusicPhase::Silent;
			if (bHasPending)
			{
				StartPending();
			}
		}
	}

	if (AudioComponent)
	{
		AudioComponent->SetVolumeMultiplier(FMath::Max(FadeVolume, StartechMusic::MinComponentVolume));
	}
}

void UStartechMusicSubsystem::SetMusicTableForTest(UMusicPlaylist* const Menu, const TArray<FMusicTableEntry>& Sectors)
{
	bUseTestTable = Menu != nullptr;
	TestMenuPlaylist = Menu;
	TestSectors = Sectors;
}
