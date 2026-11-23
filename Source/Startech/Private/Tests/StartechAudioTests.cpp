#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AmmoData.h"
#include "Audio/MusicPlaylist.h"
#include "Audio/StartechAudioSubsystem.h"
#include "Audio/StartechMusicSubsystem.h"
#include "Audio/StartechUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UI/Hub/HubSliderRowWidget.h"
#include "UI/Menu/MainMenuWidget.h"
#include "UI/Pause/PauseMenuWidget.h"
#include "UI/Settings/SettingsScreenWidget.h"
#include "EnemyData.h"
#include "LaserMountComponent.h"
#include "LaserWeaponData.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Tests/StartechTestWorld.h"
#include "WeaponEffectsComponent.h"
#include "WeaponSystemComponent.h"

namespace StartechAudioTests
{
	/** Longer than any fade in the settings, in steps like the core ticker's. */
	static void AdvanceFades(UStartechMusicSubsystem& Music, const float Seconds = 2.5f)
	{
		for (float Done = 0.f; Done < Seconds; Done += 0.05f)
		{
			Music.Advance(0.05f);
		}
	}

	/** A music player owned by the test world's bare game instance: no ticker, so the test drives the fades. */
	static UStartechMusicSubsystem* MakeMusic(const StartechTestWorld::FTestWorld& Fixture)
	{
		return NewObject<UStartechMusicSubsystem>(Fixture.GameInstance);
	}

	/** A transient playlist of small sound effects (so tests never load the long music takes). */
	static UMusicPlaylist* MakePlaylist(const TCHAR* const Name, const TArray<FString>& Tracks)
	{
		UMusicPlaylist* const Playlist = NewObject<UMusicPlaylist>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UMusicPlaylist::StaticClass(), Name));
		for (const FString& Track : Tracks)
		{
			Playlist->Tracks.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(Track)));
		}
		return Playlist;
	}

	static const TCHAR* const SmallA = TEXT("/Game/Audio/SFX/SW_Warp.SW_Warp");
	static const TCHAR* const SmallB = TEXT("/Game/Audio/SFX/SW_ChargeUp.SW_ChargeUp");

	static FString NameOf(const UObject* const Object)
	{
		return Object ? Object->GetName() : FString(TEXT("none"));
	}

	/**
	 * A player controller with a local player in the test world: widgets only run NativeOnInitialized (where the
	 * menus add their pages) with a valid player context. Detach it with ReleaseLocalPlayer.
	 */
	static APlayerController* MakeLocalPlayerController(const StartechTestWorld::FTestWorld& Fixture)
	{
		APlayerController* const Controller = Fixture.World->SpawnActor<APlayerController>();
		ULocalPlayer* const LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		LocalPlayer->PlayerController = Controller;
		Controller->Player = LocalPlayer;
		return Controller;
	}

	static void ReleaseLocalPlayer(APlayerController* const Controller)
	{
		if (Controller)
		{
			Controller->Player = nullptr;
		}
	}

	/** Every concurrency asset the sound uses. */
	static TArray<const USoundConcurrency*> GetConcurrency(const USoundBase& Sound)
	{
		TArray<const USoundConcurrency*> Out;
		for (const USoundConcurrency* const Concurrency : Sound.ConcurrencySet)
		{
			Out.Add(Concurrency);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioMusicFlowTest, "Startech.Audio.MenuAndEverySectorPlayItsMusic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioMusicFlowTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	const StartechTestWorld::FTestWorld Fixture;
	UStartechMusicSubsystem* const Music = MakeMusic(Fixture);

	// The main menu.
	Music->PlayMenuMusic();
	TestEqual(TEXT("main menu playlist"), NameOf(Music->GetPlaylist()), FString(TEXT("DA_Music_Universe")));
	TestEqual(TEXT("menu music fades in"), Music->GetPhase(), EMusicPhase::FadingIn);
	AdvanceFades(*Music);
	TestEqual(TEXT("then plays"), Music->GetPhase(), EMusicPhase::Playing);
	TestTrue(TEXT("a take of that playlist"), Music->GetPlaylist()->Tracks.ContainsByPredicate(
		[Music](const TSoftObjectPtr<USoundBase>& Each) { return Each.ToSoftObjectPath() == Music->GetTrack(); }));

	// New game: the menu music fades out across the load, then Terra's plays (start flow: transition, ready).
	Music->FadeOut();
	Music->PrepareSectorChange(TEXT("Terra"));
	Music->PlaySector(TEXT("Terra"));
	TestEqual(TEXT("the menu music finishes fading out first"), NameOf(Music->GetPlaylist()), FString(TEXT("DA_Music_Universe")));
	TestEqual(TEXT("Terra's waits"), NameOf(Music->GetPendingPlaylist()), FString(TEXT("DA_Music_Earth")));
	AdvanceFades(*Music);
	TestEqual(TEXT("Terra"), NameOf(Music->GetPlaylist()), FString(TEXT("DA_Music_Earth")));

	// Warping along the chain: fade out fully, then the next sector's fades in.
	const TArray<TPair<FName, FString>> Chain = {
		{ TEXT("Rust"), TEXT("DA_Music_Mars") }, { TEXT("Glacier"), TEXT("DA_Music_Neptune") }, { TEXT("Jove"), TEXT("DA_Music_Jupiter") },
		{ TEXT("Glacier"), TEXT("DA_Music_Neptune") }, { TEXT("Rust"), TEXT("DA_Music_Mars") }, { TEXT("Terra"), TEXT("DA_Music_Earth") } };
	for (const TPair<FName, FString>& Step : Chain)
	{
		const int32 StartsBefore = Music->GetTrackStarts();
		Music->PrepareSectorChange(Step.Key);
		TestEqual(FString::Printf(TEXT("leaving for %s fades out"), *Step.Key.ToString()), Music->GetPhase(), EMusicPhase::FadingOut);
		Music->PlaySector(Step.Key);
		TestEqual(TEXT("nothing new starts during the fade-out"), Music->GetTrackStarts(), StartsBefore);
		AdvanceFades(*Music);
		TestEqual(FString::Printf(TEXT("%s plays its playlist"), *Step.Key.ToString()), NameOf(Music->GetPlaylist()), Step.Value);
		TestEqual(TEXT("one new take"), Music->GetTrackStarts(), StartsBefore + 1);
		AdvanceFades(*Music);
		TestEqual(TEXT("faded in"), Music->GetPhase(), EMusicPhase::Playing);
	}

	// Quit to the main menu: the sector music fades out, the menu's follows.
	Music->FadeOut();
	Music->PlayMenuMusic();
	AdvanceFades(*Music);
	TestEqual(TEXT("back in the menu"), NameOf(Music->GetPlaylist()), FString(TEXT("DA_Music_Universe")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioSharedPlaylistTest, "Startech.Audio.SharedPlaylistIsNotRestarted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioSharedPlaylistTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	const StartechTestWorld::FTestWorld Fixture;
	UStartechMusicSubsystem* const Music = MakeMusic(Fixture);

	UMusicPlaylist* const Menu = MakePlaylist(TEXT("TestMenu"), { SmallA });
	UMusicPlaylist* const Shared = MakePlaylist(TEXT("TestShared"), { SmallA, SmallB });
	UMusicPlaylist* const Other = MakePlaylist(TEXT("TestOther"), { SmallB });
	FMusicTableEntry X, Y, Z;
	X.SectorId = TEXT("X");
	X.Playlist = Shared;
	Y.SectorId = TEXT("Y");
	Y.Playlist = Shared;
	Z.SectorId = TEXT("Z");
	Z.Playlist = Other;
	Music->SetMusicTableForTest(Menu, { X, Y, Z });

	Music->PlaySector(TEXT("X"));
	AdvanceFades(*Music);
	const FSoftObjectPath Take = Music->GetTrack();
	TestEqual(TEXT("one take started"), Music->GetTrackStarts(), 1);

	// X → Y share the playlist: no fade, no new take, the same take keeps playing.
	Music->PrepareSectorChange(TEXT("Y"));
	TestEqual(TEXT("no fade-out towards a sector sharing the playlist"), Music->GetPhase(), EMusicPhase::Playing);
	Music->PlaySector(TEXT("Y"));
	AdvanceFades(*Music);
	TestEqual(TEXT("not restarted"), Music->GetTrackStarts(), 1);
	TestEqual(TEXT("the same take plays on"), Music->GetTrack(), Take);

	// A respawn regenerating the same sector (Y → Y) doesn't restart it either.
	Music->PrepareSectorChange(TEXT("Y"));
	Music->PlaySector(TEXT("Y"));
	TestEqual(TEXT("still not restarted"), Music->GetTrackStarts(), 1);

	// Y → Z: a different playlist fades out, then in.
	Music->PrepareSectorChange(TEXT("Z"));
	TestEqual(TEXT("fades out towards another playlist"), Music->GetPhase(), EMusicPhase::FadingOut);
	Music->PlaySector(TEXT("Z"));
	AdvanceFades(*Music);
	TestEqual(TEXT("then Z's"), NameOf(Music->GetPlaylist()), NameOf(Other));
	TestEqual(TEXT("one new take"), Music->GetTrackStarts(), 2);

	// A sector with no music: silence after the fade.
	Music->PrepareSectorChange(TEXT("Nowhere"));
	Music->PlaySector(TEXT("Nowhere"));
	AdvanceFades(*Music);
	TestEqual(TEXT("an unlisted sector is silent"), Music->GetPhase(), EMusicPhase::Silent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioNextTakeTest, "Startech.Audio.PlaylistPlaysAnotherTakeWhenOneEnds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioNextTakeTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	const StartechTestWorld::FTestWorld Fixture;
	UStartechMusicSubsystem* const Music = MakeMusic(Fixture);

	UMusicPlaylist* const Two = MakePlaylist(TEXT("TestTwo"), { SmallA, SmallB });
	UMusicPlaylist* const One = MakePlaylist(TEXT("TestOne"), { SmallA });
	FMusicTableEntry Entry;
	Entry.SectorId = TEXT("Two");
	Entry.Playlist = Two;
	FMusicTableEntry Single;
	Single.SectorId = TEXT("One");
	Single.Playlist = One;
	Music->SetMusicTableForTest(One, { Entry, Single });

	Music->PlaySector(TEXT("Two"));
	AdvanceFades(*Music);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const FSoftObjectPath Before = Music->GetTrack();
		Music->HandleTrackFinished();
		TestNotEqual(TEXT("never the same take twice in a row"), Music->GetTrack(), Before);
		TestEqual(TEXT("keeps playing at full volume"), Music->GetPhase(), EMusicPhase::Playing);
	}
	TestEqual(TEXT("21 takes"), Music->GetTrackStarts(), 21);

	// A one-take playlist loops that take.
	Music->PrepareSectorChange(TEXT("One"));
	Music->PlaySector(TEXT("One"));
	AdvanceFades(*Music, 5.f);
	const int32 Starts = Music->GetTrackStarts();
	Music->HandleTrackFinished();
	TestEqual(TEXT("a single take restarts"), Music->GetTrackStarts(), Starts + 1);
	TestEqual(TEXT("the same take"), Music->GetTrack(), FSoftObjectPath(SmallA));

	// A take ending during a fade-out (its own stop) starts nothing.
	Music->FadeOut();
	Music->HandleTrackFinished();
	TestEqual(TEXT("no take during a fade-out"), Music->GetTrackStarts(), Starts + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioGroupsTest, "Startech.Audio.SoundsUseTheirGroupsAndLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioGroupsTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	USoundClass* const Master = Settings.MasterClass.LoadSynchronous();
	USoundClass* const SFX = Settings.SFXClass.LoadSynchronous();
	USoundClass* const Weapons = Settings.WeaponsClass.LoadSynchronous();
	USoundClass* const Explosions = Settings.ExplosionsClass.LoadSynchronous();
	USoundClass* const MusicClass = Settings.MusicClass.LoadSynchronous();
	USoundClass* const MenuMusicClass = Settings.MenuMusicClass.LoadSynchronous();
	if (!TestTrue(TEXT("every sound group loads"), Master && SFX && Weapons && Explosions && MusicClass && MenuMusicClass))
	{
		return false;
	}

	// The tree: Master > Music, MenuMusic, SFX > Weapons, Explosions.
	TestTrue(TEXT("Master's children"), Master->ChildClasses.Contains(MusicClass) && Master->ChildClasses.Contains(MenuMusicClass) && Master->ChildClasses.Contains(SFX));
	TestTrue(TEXT("SFX's children"), SFX->ChildClasses.Contains(Weapons) && SFX->ChildClasses.Contains(Explosions));
	TestTrue(TEXT("parents"), Weapons->ParentClass == SFX && Explosions->ParentClass == SFX && SFX->ParentClass == Master && MusicClass->ParentClass == Master);

	// The volume mix: every group, Master's for its children too; the hub mix silences SFX and its children.
	const USoundMix* const Volumes = Settings.VolumeMix.LoadSynchronous();
	const USoundMix* const Hub = Settings.HubMix.LoadSynchronous();
	if (TestNotNull(TEXT("SMix_Volumes"), Volumes) && TestNotNull(TEXT("SMix_Hub"), Hub))
	{
		for (const USoundClass* const Class : { Master, MusicClass, MenuMusicClass, SFX, Weapons, Explosions })
		{
			const FSoundClassAdjuster* const Adjuster = Volumes->SoundClassEffects.FindByPredicate(
				[Class](const FSoundClassAdjuster& Each) { return Each.SoundClassObject == Class; });
			TestNotNull(FString::Printf(TEXT("SMix_Volumes adjusts %s"), *Class->GetName()), Adjuster);
			if (Adjuster)
			{
				TestEqual(FString::Printf(TEXT("%s applies to children only for Master"), *Class->GetName()), static_cast<bool>(Adjuster->bApplyToChildren), Class == Master);
			}
		}
		TestTrue(TEXT("SMix_Hub silences SFX and its children"), Hub->SoundClassEffects.ContainsByPredicate([SFX](const FSoundClassAdjuster& Each)
		{
			return Each.SoundClassObject == SFX && Each.VolumeAdjuster == 0.f && Each.bApplyToChildren;
		}));
	}
	const FStartechVolumes& Defaults = Settings.DefaultVolumes;
	TestEqual(TEXT("defaults: master 100%"), Defaults.Master, 1.f);
	TestEqual(TEXT("defaults: music 70%"), Defaults.Music, 0.7f);
	TestEqual(TEXT("defaults: menu music 70%"), Defaults.MenuMusic, 0.7f);
	TestEqual(TEXT("defaults: weapons 80%"), Defaults.Weapons, 0.8f);
	TestEqual(TEXT("defaults: explosions 90%"), Defaults.Explosions, 0.9f);
	TestEqual(TEXT("Weapons adjuster gets the Weapons volume"), UStartechAudioSubsystem::GetAdjusterVolume(Defaults, *Weapons), 0.8f);
	TestEqual(TEXT("SFX has no slider: 1"), UStartechAudioSubsystem::GetAdjusterVolume(Defaults, *SFX), 1.f);

	const auto CheckSound = [this](const TCHAR* const What, const USoundBase* const Sound, const USoundClass* const Class,
		const int32 MaxCount, const bool bSpatial)
	{
		if (!TestNotNull(What, Sound))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("%s: group"), What), NameOf(Sound->GetSoundClass()), NameOf(Class));
		const TArray<const USoundConcurrency*> Concurrency = GetConcurrency(*Sound);
		TestEqual(FString::Printf(TEXT("%s: one concurrency limit"), What), Concurrency.Num(), 1);
		if (Concurrency.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("%s: at most %d at once"), What, MaxCount), Concurrency[0]->Concurrency.MaxCount, MaxCount);
			TestEqual(FString::Printf(TEXT("%s: the oldest stops"), What), Concurrency[0]->Concurrency.ResolutionRule, EMaxConcurrentResolutionRule::StopOldest);
		}
		TestEqual(FString::Printf(TEXT("%s: attenuation"), What), Sound->AttenuationSettings != nullptr, bSpatial);
	};

	// Lasers: every laser ammo, the player's and the enemies'.
	TArray<const UAmmoData*> LaserAmmo;
	for (const TCHAR* const Path : { TEXT("/Game/Data/AmmoAsset/DA_1xAmmo.DA_1xAmmo"), TEXT("/Game/Data/AmmoAsset/DA_2x_Ammo.DA_2x_Ammo"),
		TEXT("/Game/Data/AmmoAsset/DA_x3_Ammo.DA_x3_Ammo"), TEXT("/Game/Data/AmmoAsset/DA_x4_Ammo.DA_x4_Ammo") })
	{
		LaserAmmo.Add(LoadObject<UAmmoData>(nullptr, Path));
	}
	for (const TCHAR* const Path : { TEXT("/Game/Data/EnemyData/DA_Enemy_Drifter.DA_Enemy_Drifter"),
		TEXT("/Game/Data/EnemyData/DA_Enemy_Chaser.DA_Enemy_Chaser"), TEXT("/Game/Data/EnemyData/DA_Enemy_Ranged.DA_Enemy_Ranged") })
	{
		const UEnemyData* const Enemy = LoadObject<UEnemyData>(nullptr, Path);
		LaserAmmo.Add(Enemy ? Enemy->LaserAmmo.Get() : nullptr);
	}
	for (const UAmmoData* const Ammo : LaserAmmo)
	{
		if (!TestNotNull(TEXT("laser ammo"), Ammo))
		{
			continue;
		}
		CheckSound(*FString::Printf(TEXT("%s start"), *Ammo->GetName()), Ammo->StartSound, Weapons, 8, true);
		CheckSound(*FString::Printf(TEXT("%s hit"), *Ammo->GetName()), Ammo->HitSound, Weapons, 8, true);
		TestNull(*FString::Printf(TEXT("%s has no travel sound"), *Ammo->GetName()), Ammo->TravelSound.Get());
		TestTrue(*FString::Printf(TEXT("%s start is trimmed"), *Ammo->GetName()), Ammo->StartSound && Ammo->StartSound->GetDuration() <= 0.3f);
		TestTrue(*FString::Printf(TEXT("%s hit is trimmed"), *Ammo->GetName()), Ammo->HitSound && Ammo->HitSound->GetDuration() <= 0.3f);
	}

	// The drainer's beam loops.
	for (const TCHAR* const Path : { TEXT("/Game/Data/AmmoAsset/DA_1xDrainShield.DA_1xDrainShield"), TEXT("/Game/Data/AmmoAsset/DA_2xDrainShield.DA_2xDrainShield") })
	{
		const UAmmoData* const Drain = LoadObject<UAmmoData>(nullptr, Path);
		if (TestNotNull(TEXT("drain ammo"), Drain))
		{
			CheckSound(TEXT("beam loop"), Drain->TravelSound, Weapons, 4, true);
			const USoundWave* const Wave = Cast<USoundWave>(Drain->TravelSound);
			TestTrue(TEXT("the beam sound loops"), Wave && Wave->bLooping);
		}
	}

	// Explosions, the charge-up and the warp.
	CheckSound(TEXT("death explosion"), UStartechSettings::Get().Death.DefaultEffects.DeathSound.LoadSynchronous(), Explosions, 4, true);
	CheckSound(TEXT("charge-up"), UStartechSettings::Get().ChargedShot.ChargeSound.LoadSynchronous(), Weapons, 4, true);
	CheckSound(TEXT("warp"), Settings.WarpSound.LoadSynchronous(), SFX, 1, false);

	// Every sector of the chain and the menu have a playlist with takes.
	for (const FSectorTierAssignment& Sector : UStartechSettings::Get().SectorTiers)
	{
		const FSectorMusic* const Entry = Settings.SectorMusic.FindByPredicate([&Sector](const FSectorMusic& Each) { return Each.SectorId == Sector.SectorId; });
		const UMusicPlaylist* const Playlist = Entry ? Entry->Playlist.LoadSynchronous() : nullptr;
		TestTrue(FString::Printf(TEXT("%s has music"), *Sector.SectorId.ToString()), Playlist && Playlist->Tracks.Num() > 0);
	}
	const UMusicPlaylist* const MenuPlaylist = Settings.MenuMusic.LoadSynchronous();
	TestTrue(TEXT("the menu has music"), MenuPlaylist && MenuPlaylist->Tracks.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioVolleyTest, "Startech.Audio.OneStartSoundPerVolley",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioVolleyTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	UClass* const PlayerClass = LoadClass<AStartechShipPawn>(nullptr, TEXT("/Game/Blueprints/BP_ShipPawn.BP_ShipPawn_C"));
	ULaserWeaponData* const Weapon = LoadObject<ULaserWeaponData>(nullptr, TEXT("/Game/Data/WeaponAsset/DA_BasicWeapon.DA_BasicWeapon"));
	UAmmoData* const Ammo = LoadObject<UAmmoData>(nullptr, TEXT("/Game/Data/AmmoAsset/DA_1xAmmo.DA_1xAmmo"));
	if (!TestTrue(TEXT("assets load"), PlayerClass && Weapon && Ammo))
	{
		return false;
	}

	const StartechTestWorld::FTestWorld Fixture;
	AStartechShipPawn* const Ship = Fixture.SpawnPlayer(PlayerClass, FVector(0.f, 0.f, 1000.f));
	UWeaponEffectsComponent* const Effects = Ship ? Ship->FindComponentByClass<UWeaponEffectsComponent>() : nullptr;
	UWeaponSystemComponent* const WeaponSystem = Ship ? Ship->FindComponentByClass<UWeaponSystemComponent>() : nullptr;
	if (!TestTrue(TEXT("player ship with effects and weapons"), Effects && WeaponSystem))
	{
		return false;
	}
	ULaserMountComponent* const Left = Ship->GetLaserMountLeft();
	ULaserMountComponent* const Right = Ship->GetLaserMountRight();
	Left->SetWeapon(Weapon);
	Right->SetWeapon(Weapon);
	WeaponSystem->SelectAmmo(Ammo);

	const FVector Aim = Ship->GetActorLocation() + FVector(5000.f, 0.f, 0.f);
	const int32 Before = Effects->GetStartSoundsPlayed();

	// Both mounts fire in the same frame: one volley, one sound.
	Left->UpdateFiring(true, Aim, 0.01f);
	Right->UpdateFiring(true, Aim, 0.01f);
	TestEqual(TEXT("two mounts firing together play one start sound"), Effects->GetStartSoundsPlayed(), Before + 1);

	// The next volley (after the 4/s cooldown) plays again.
	Fixture.Tick(0.3f);
	Left->UpdateFiring(true, Aim, 0.3f);
	Right->UpdateFiring(true, Aim, 0.3f);
	TestEqual(TEXT("the next volley plays its own"), Effects->GetStartSoundsPlayed(), Before + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioSavedVolumesTest, "Startech.Audio.SavedVolumesAreAppliedAfterARestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioSavedVolumesTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	UStartechUserSettings* const UserSettings = UStartechUserSettings::Get();
	if (!TestNotNull(TEXT("the engine's user settings are UStartechUserSettings (GameUserSettingsClassName)"), UserSettings))
	{
		return false;
	}
	// This machine's settings, put back at the end.
	const bool bHadSaved = UserSettings->HasSavedVolumes();
	const FStartechVolumes Before = UserSettings->GetVolumes();

	const StartechTestWorld::FTestWorld Fixture;
	UStartechAudioSubsystem* const Audio = NewObject<UStartechAudioSubsystem>(Fixture.GameInstance);
	FStartechVolumes Changed;
	Changed.Master = 0.55f;
	Changed.Music = 0.25f;
	Changed.MenuMusic = 0.4f;
	Changed.Weapons = 0.35f;
	Changed.Explosions = 0.6f;
	Audio->SetVolumes(Changed);
	Audio->SaveVolumes();

	// A restart: a fresh settings object reads GameUserSettings.ini again, and the game starts with what it holds.
	UStartechUserSettings* const Fresh = NewObject<UStartechUserSettings>(GetTransientPackage());
	Fresh->LoadSettings(true);
	TestTrue(TEXT("the saved volumes are read back from GameUserSettings.ini"), Fresh->GetVolumes() == Changed);
	TestTrue(TEXT("and are what the game starts with"), UStartechAudioSubsystem::LoadSavedVolumes() == Changed);

	// Forgetting them brings the defaults back.
	UserSettings->ClearSavedVolumes();
	TestTrue(TEXT("no saved volumes: the defaults"), UStartechAudioSubsystem::LoadSavedVolumes() == UStartechSettings::Get().Audio.DefaultVolumes);

	if (bHadSaved)
	{
		UserSettings->SetVolumes(Before);
	}
	UserSettings->SaveSettings();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioMenuMusicSliderTest, "Startech.Audio.OnlyTheMainMenuShowsTheMenuMusicSlider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioMenuMusicSliderTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	const StartechTestWorld::FTestWorld Fixture;
	const UClass* const MainClass = UStartechSettings::Get().MainMenuWidgetClass.LoadSynchronous();
	const UClass* const PauseClass = UStartechSettings::Get().PauseMenuWidgetClass.LoadSynchronous();
	if (!TestTrue(TEXT("menu classes load"), MainClass && PauseClass))
	{
		return false;
	}
	// The real Blueprints: their pages are added in NativeOnInitialized, which CreateWidget runs.
	APlayerController* const Controller = MakeLocalPlayerController(Fixture);
	ON_SCOPE_EXIT { ReleaseLocalPlayer(Controller); };
	UMainMenuWidget* const Main = CreateWidget<UMainMenuWidget>(Controller, const_cast<UClass*>(MainClass));
	UPauseMenuWidget* const Pause = CreateWidget<UPauseMenuWidget>(Controller, const_cast<UClass*>(PauseClass));
	USettingsScreenWidget* const MainPage = Main ? Main->GetSettingsScreen() : nullptr;
	USettingsScreenWidget* const PausePage = Pause ? Pause->GetSettingsScreen() : nullptr;
	if (!TestTrue(TEXT("both menus make a Settings page"), MainPage && PausePage))
	{
		return false;
	}
	TestNotNull(TEXT("the main menu has a Settings button"), Main->GetSettingsButton());
	TestNotNull(TEXT("the pause menu has a Settings button"), Pause->GetSettingsButton());

	const auto Shown = [](const USettingsScreenWidget& Page)
	{
		TArray<FString> Labels;
		for (const UHubSliderRowWidget* const Slider : Page.GetSliders())
		{
			if (Slider->GetVisibility() != ESlateVisibility::Collapsed)
			{
				Labels.Add(Slider->GetLabel().ToString());
			}
		}
		return FString::Join(Labels, TEXT(", "));
	};
	TestTrue(TEXT("main menu: Menu music shown"), MainPage->IsMenuMusicShown());
	TestFalse(TEXT("pause menu: Menu music hidden"), PausePage->IsMenuMusicShown());
	TestEqual(TEXT("main menu sliders"), Shown(*MainPage), FString(TEXT("Master, Music, Menu music, Weapons, Explosions")));
	TestEqual(TEXT("pause menu sliders"), Shown(*PausePage), FString(TEXT("Master, Music, Weapons, Explosions")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechAudioResetTest, "Startech.Audio.ResetRestoresTheDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FStartechAudioResetTest::RunTest(const FString& Parameters)
{
	using namespace StartechAudioTests;
	const StartechTestWorld::FTestWorld Fixture;
	APlayerController* const Controller = MakeLocalPlayerController(Fixture);
	ON_SCOPE_EXIT { ReleaseLocalPlayer(Controller); };
	UPauseMenuWidget* const Pause = CreateWidget<UPauseMenuWidget>(Controller, UStartechSettings::Get().PauseMenuWidgetClass.LoadSynchronous());
	USettingsScreenWidget* const Page = Pause ? Pause->GetSettingsScreen() : nullptr;
	if (!TestNotNull(TEXT("the pause menu's Settings page"), Page))
	{
		return false;
	}
	for (UHubSliderRowWidget* const Slider : Page->GetSliders())
	{
		Slider->SetValue(0.13f);
	}
	Page->ResetToDefaults();

	const FStartechVolumes& Defaults = UStartechSettings::Get().Audio.DefaultVolumes;
	for (const EStartechVolume Volume : { EStartechVolume::Master, EStartechVolume::Music, EStartechVolume::MenuMusic,
		EStartechVolume::Weapons, EStartechVolume::Explosions })
	{
		const UHubSliderRowWidget* const Slider = Page->GetSlider(Volume);
		TestTrue(FString::Printf(TEXT("slider %d back to its default"), static_cast<int32>(Volume)),
			Slider && FMath::IsNearlyEqual(Slider->GetValue(), Defaults.Get(Volume), KINDA_SMALL_NUMBER));
	}
	TestEqual(TEXT("Master reads 100%"), Page->GetSlider(EStartechVolume::Master)->GetValueText(), FString(TEXT("100%")));
	TestEqual(TEXT("Music reads 70%"), Page->GetSlider(EStartechVolume::Music)->GetValueText(), FString(TEXT("70%")));
	TestEqual(TEXT("Weapons reads 80%"), Page->GetSlider(EStartechVolume::Weapons)->GetValueText(), FString(TEXT("80%")));
	TestEqual(TEXT("Explosions reads 90%"), Page->GetSlider(EStartechVolume::Explosions)->GetValueText(), FString(TEXT("90%")));

	// The audio side: Reset gives the subsystem the defaults (SetVolumes is what the page calls).
	UStartechAudioSubsystem* const Audio = NewObject<UStartechAudioSubsystem>(Fixture.GameInstance);
	FStartechVolumes Odd;
	Odd.Set(EStartechVolume::Weapons, 0.1f);
	Audio->SetVolumes(Odd);
	Audio->SetVolumes(Defaults);
	TestTrue(TEXT("the subsystem holds the defaults"), Audio->GetVolumes() == Defaults);
	TestEqual(TEXT("Set clamps"), [] { FStartechVolumes V; V.Set(EStartechVolume::Master, 1.7f); return V.Master; }(), 1.f);
	return true;
}

#endif
