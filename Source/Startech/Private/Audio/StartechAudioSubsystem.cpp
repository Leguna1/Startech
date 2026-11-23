#include "Audio/StartechAudioSubsystem.h"

#include "Audio/StartechUserSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY(LogStartechAudio);

float FStartechVolumes::Get(const EStartechVolume Volume) const
{
	switch (Volume)
	{
	case EStartechVolume::Music: return Music;
	case EStartechVolume::MenuMusic: return MenuMusic;
	case EStartechVolume::Weapons: return Weapons;
	case EStartechVolume::Explosions: return Explosions;
	default: return Master;
	}
}

void FStartechVolumes::Set(const EStartechVolume Volume, const float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
	switch (Volume)
	{
	case EStartechVolume::Music: Music = Clamped; break;
	case EStartechVolume::MenuMusic: MenuMusic = Clamped; break;
	case EStartechVolume::Weapons: Weapons = Clamped; break;
	case EStartechVolume::Explosions: Explosions = Clamped; break;
	default: Master = Clamped; break;
	}
}

bool FStartechVolumes::operator==(const FStartechVolumes& Other) const
{
	return Master == Other.Master && Music == Other.Music && MenuMusic == Other.MenuMusic
		&& Weapons == Other.Weapons && Explosions == Other.Explosions;
}

UStartechAudioSubsystem* UStartechAudioSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UStartechAudioSubsystem>() : nullptr;
}

void UStartechAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Volumes = LoadSavedVolumes();
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UStartechAudioSubsystem::HandleWorldActorsInitialized);
}

void UStartechAudioSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	Super::Deinitialize();
}

UWorld* UStartechAudioSubsystem::GetPlayWorld() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void UStartechAudioSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	UWorld* const World = Params.World;
	if (World && World->IsGameWorld() && World->GetGameInstance() == GetGameInstance())
	{
		// A new map: the hub can't be open in it yet.
		bHubMixPushed = false;
		ApplyToWorld(*World, 0.f);
	}
}

float UStartechAudioSubsystem::GetAdjusterVolume(const FStartechVolumes& InVolumes, const USoundClass& Class)
{
	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	const auto Is = [&Class](const TSoftObjectPtr<USoundClass>& Soft) { return Soft.ToSoftObjectPath() == FSoftObjectPath(&Class); };
	if (Is(Settings.MasterClass))
	{
		return InVolumes.Master;
	}
	if (Is(Settings.MusicClass))
	{
		return InVolumes.Music;
	}
	if (Is(Settings.MenuMusicClass))
	{
		return InVolumes.MenuMusic;
	}
	if (Is(Settings.WeaponsClass))
	{
		return InVolumes.Weapons;
	}
	if (Is(Settings.ExplosionsClass))
	{
		return InVolumes.Explosions;
	}
	// SFX and any later group with no slider of its own: Master alone.
	return 1.f;
}

void UStartechAudioSubsystem::SetVolumes(const FStartechVolumes& InVolumes, const float FadeSeconds)
{
	Volumes = InVolumes;
	if (UWorld* const World = GetPlayWorld())
	{
		ApplyToWorld(*World, FadeSeconds);
	}
}

void UStartechAudioSubsystem::ApplyToWorld(UWorld& World, const float FadeSeconds)
{
	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	USoundMix* const Mix = Settings.VolumeMix.LoadSynchronous();
	if (!Mix)
	{
		static bool bWarned = false;
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogStartechAudio, Error, TEXT("UStartechSettings::Audio.VolumeMix is not set or missing: no volume control."));
		}
		return;
	}

	for (const TSoftObjectPtr<USoundClass>& Soft : { Settings.MasterClass, Settings.MusicClass, Settings.MenuMusicClass,
		Settings.SFXClass, Settings.WeaponsClass, Settings.ExplosionsClass })
	{
		if (USoundClass* const Class = Soft.LoadSynchronous())
		{
			const bool bChildren = Soft == Settings.MasterClass;
			UGameplayStatics::SetSoundMixClassOverride(&World, Mix, Class, GetAdjusterVolume(Volumes, *Class), 1.f, FadeSeconds, bChildren);
		}
	}
	// Popped first, so pushing it again into a device that kept it never stacks it.
	UGameplayStatics::PopSoundMixModifier(&World, Mix);
	UGameplayStatics::PushSoundMixModifier(&World, Mix);

	UE_LOG(LogStartechAudio, Log, TEXT("[Audio] volumes in %s: master %.2f music %.2f menu %.2f weapons %.2f explosions %.2f"),
		*World.GetName(), Volumes.Master, Volumes.Music, Volumes.MenuMusic, Volumes.Weapons, Volumes.Explosions);
}

FStartechVolumes UStartechAudioSubsystem::LoadSavedVolumes()
{
	const UStartechUserSettings* const UserSettings = UStartechUserSettings::Get();
	return UserSettings ? UserSettings->GetVolumes() : UStartechSettings::Get().Audio.DefaultVolumes;
}

void UStartechAudioSubsystem::SaveVolumes() const
{
	UStartechUserSettings* const UserSettings = UStartechUserSettings::Get();
	if (!UserSettings)
	{
		UE_LOG(LogStartechAudio, Error, TEXT("GameUserSettingsClassName is not UStartechUserSettings: the volumes are not saved."));
		return;
	}
	UserSettings->SetVolumes(Volumes);
	UserSettings->SaveSettings();
	UE_LOG(LogStartechAudio, Log, TEXT("[Audio] volumes saved: master %.2f music %.2f menu %.2f weapons %.2f explosions %.2f"),
		Volumes.Master, Volumes.Music, Volumes.MenuMusic, Volumes.Weapons, Volumes.Explosions);
}

void UStartechAudioSubsystem::PlayPreview(const EStartechVolume Volume) const
{
	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	const TSoftObjectPtr<USoundBase>* const Soft = Volume == EStartechVolume::Weapons ? &Settings.WeaponsPreviewSound
		: Volume == EStartechVolume::Explosions ? &Settings.ExplosionsPreviewSound : nullptr;
	USoundBase* const Sound = Soft && !Soft->IsNull() ? Soft->LoadSynchronous() : nullptr;
	if (UWorld* const World = Sound ? GetPlayWorld() : nullptr)
	{
		// A UI sound, so it is heard in the paused pause menu; its class still applies the group's new volume.
		UGameplayStatics::PlaySound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, nullptr, true);
		UE_LOG(LogStartechAudio, Log, TEXT("[Audio] preview %s (%s) at %.2f"), *Sound->GetName(),
			*StaticEnum<EStartechVolume>()->GetNameStringByValue(static_cast<int64>(Volume)), Volumes.Get(Volume));
	}
}

void UStartechAudioSubsystem::SetHubOpen(const bool bOpen)
{
	if (bOpen == bHubMixPushed)
	{
		return;
	}
	UWorld* const World = GetPlayWorld();
	USoundMix* const Mix = UStartechSettings::Get().Audio.HubMix.LoadSynchronous();
	if (!World || !Mix)
	{
		return;
	}
	if (bOpen)
	{
		UGameplayStatics::PushSoundMixModifier(World, Mix);
	}
	else
	{
		UGameplayStatics::PopSoundMixModifier(World, Mix);
	}
	bHubMixPushed = bOpen;
	UE_LOG(LogStartechAudio, Log, TEXT("[Audio] hub mix %s."), bOpen ? TEXT("pushed (SFX fading out)") : TEXT("popped"));
}

void UStartechAudioSubsystem::PlayWarpSound(const UObject* const WorldContext) const
{
	const TSoftObjectPtr<USoundBase>& Soft = UStartechSettings::Get().Audio.WarpSound;
	if (USoundBase* const Sound = Soft.IsNull() ? nullptr : Soft.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(WorldContext, Sound);
	}
}
