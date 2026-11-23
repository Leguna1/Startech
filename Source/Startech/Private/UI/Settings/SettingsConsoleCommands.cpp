#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Audio/StartechAudioSubsystem.h"
#include "Audio/StartechMusicSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubSliderRowWidget.h"
#include "UI/Menu/MainMenuWidget.h"
#include "UI/Pause/PauseMenuWidget.h"
#include "UI/Settings/SettingsScreenWidget.h"

/**
 * Settings page test aids (non-shipping):
 *   Startech.Test.Settings [log]                     the menu up, whether the page is open, every row (label, value,
 *                                                    shown), the volumes in use, the music's fade, and whether paused
 *   Startech.Test.Settings open|back|reset           a real mouse click on the menu's Settings button, Back, or Reset
 *   Startech.Test.Settings drag <Volume> <Percent>   a real mouse drag of that slider's thumb to Percent
 *                                                    (Volume: Master, Music, MenuMusic, Weapons, Explosions)
 */
namespace SettingsConsole
{
	struct FMenu
	{
		const TCHAR* Name = TEXT("none");
		USettingsScreenWidget* Screen = nullptr;
		UHubButtonWidget* SettingsButton = nullptr;
	};

	static FMenu FindMenu(UWorld* const World)
	{
		FMenu Menu;
		TArray<UUserWidget*> Found;
		if (!World)
		{
			return Menu;
		}
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, UPauseMenuWidget::StaticClass(), false);
		for (UUserWidget* const Widget : Found)
		{
			if (const UPauseMenuWidget* const Pause = Cast<UPauseMenuWidget>(Widget); Pause && Pause->IsInViewport())
			{
				return { TEXT("pause menu"), Pause->GetSettingsScreen(), Pause->GetSettingsButton() };
			}
		}
		Found.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Found, UMainMenuWidget::StaticClass(), false);
		for (UUserWidget* const Widget : Found)
		{
			if (const UMainMenuWidget* const Main = Cast<UMainMenuWidget>(Widget); Main && Main->IsInViewport())
			{
				return { TEXT("main menu"), Main->GetSettingsScreen(), Main->GetSettingsButton() };
			}
		}
		return Menu;
	}

	static FVector2D Centre(const UWidget& Widget)
	{
		const FGeometry& Geometry = Widget.GetCachedGeometry();
		return FVector2D(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f);
	}

	static void MouseAt(const FVector2D& Position, const bool bDown, const bool bUp, const bool bHeld)
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const FModifierKeysState NoModifiers;
		const TSet<FKey> Held = bHeld ? TSet<FKey>{ EKeys::LeftMouseButton } : TSet<FKey>();
		Slate.SetCursorPos(Position);
		Slate.ProcessMouseMoveEvent(FPointerEvent(0, Position, Position, Held, EKeys::Invalid, 0.f, NoModifiers));
		if (bDown)
		{
			Slate.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(0, Position, Position, { EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0.f, NoModifiers));
		}
		if (bUp)
		{
			Slate.ProcessMouseButtonUpEvent(FPointerEvent(0, Position, Position, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, NoModifiers));
		}
	}

	static bool Click(const UWidget* const Widget)
	{
		if (!Widget || !FSlateApplication::IsInitialized() || Widget->GetCachedGeometry().GetAbsoluteSize().IsNearlyZero())
		{
			return false;
		}
		MouseAt(Centre(*Widget), true, true, false);
		return true;
	}

	static bool ParseVolume(const FString& Text, EStartechVolume& Out)
	{
		const UEnum* const Enum = StaticEnum<EStartechVolume>();
		const int64 Value = Enum->GetValueByNameString(Text);
		if (Value == INDEX_NONE)
		{
			return false;
		}
		Out = static_cast<EStartechVolume>(Value);
		return true;
	}

	static void Log(const FMenu& Menu, UWorld* const World)
	{
		const USettingsScreenWidget* const Screen = Menu.Screen;
		UE_LOG(LogStartechAudio, Display, TEXT("[Settings] menu=%s page=%s paused=%s"), Menu.Name,
			!Screen ? TEXT("missing") : Screen->IsOpen() ? TEXT("OPEN") : TEXT("closed"),
			World && UGameplayStatics::IsGamePaused(World) ? TEXT("yes") : TEXT("no"));
		if (Screen)
		{
			for (const UHubSliderRowWidget* const Slider : Screen->GetSliders())
			{
				UE_LOG(LogStartechAudio, Display, TEXT("[Settings]   %s %s%s"), *Slider->GetLabel().ToString(), *Slider->GetValueText(),
					Slider->GetVisibility() == ESlateVisibility::Collapsed ? TEXT(" (hidden)") : TEXT(""));
			}
		}
		if (const UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(World))
		{
			const FStartechVolumes& Volumes = Audio->GetVolumes();
			const FStartechVolumes Saved = UStartechAudioSubsystem::LoadSavedVolumes();
			UE_LOG(LogStartechAudio, Display, TEXT("[Settings] in use: master %.2f music %.2f menu %.2f weapons %.2f explosions %.2f; saved: master %.2f music %.2f menu %.2f weapons %.2f explosions %.2f"),
				Volumes.Master, Volumes.Music, Volumes.MenuMusic, Volumes.Weapons, Volumes.Explosions,
				Saved.Master, Saved.Music, Saved.MenuMusic, Saved.Weapons, Saved.Explosions);
		}
	}

	static void Settings(const TArray<FString>& Args, UWorld* const World)
	{
		const FMenu Menu = FindMenu(World);
		const FString Verb = Args.IsEmpty() ? TEXT("log") : Args[0];
		if (Verb.Equals(TEXT("open"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogStartechAudio, Display, TEXT("[Settings] clicked Settings in the %s: %s"), Menu.Name, Click(Menu.SettingsButton) ? TEXT("yes") : TEXT("NO"));
		}
		else if (Verb.Equals(TEXT("back"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogStartechAudio, Display, TEXT("[Settings] clicked Back: %s"), Click(Menu.Screen ? Menu.Screen->GetBackButton() : nullptr) ? TEXT("yes") : TEXT("NO"));
		}
		else if (Verb.Equals(TEXT("reset"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogStartechAudio, Display, TEXT("[Settings] clicked Reset to defaults: %s"), Click(Menu.Screen ? Menu.Screen->GetResetButton() : nullptr) ? TEXT("yes") : TEXT("NO"));
		}
		else if (Verb.Equals(TEXT("drag"), ESearchCase::IgnoreCase))
		{
			EStartechVolume Volume;
			UHubSliderRowWidget* const Slider = Menu.Screen && Args.Num() >= 3 && ParseVolume(Args[1], Volume) ? Menu.Screen->GetSlider(Volume) : nullptr;
			if (!Slider || !FSlateApplication::IsInitialized())
			{
				UE_LOG(LogStartechAudio, Warning, TEXT("Usage: Startech.Test.Settings drag <Master|Music|MenuMusic|Weapons|Explosions> <Percent>, with the page open."));
				return;
			}
			const float Target = FMath::Clamp(FCString::Atof(*Args[2]) / 100.f, 0.f, 1.f);
			const float From = Slider->GetValue();
			// Down on the thumb, a few moves along the track, up at the target: a real drag.
			MouseAt(Slider->GetAbsoluteTrackPoint(From), true, false, false);
			for (int32 Step = 1; Step <= 4; ++Step)
			{
				MouseAt(Slider->GetAbsoluteTrackPoint(FMath::Lerp(From, Target, Step / 4.f)), false, false, true);
			}
			MouseAt(Slider->GetAbsoluteTrackPoint(Target), false, true, false);
			UE_LOG(LogStartechAudio, Display, TEXT("[Settings] dragged %s from %.0f%% to %s"), *Args[1], From * 100.f, *Slider->GetValueText());
		}
		else
		{
			Log(Menu, World);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs SettingsCommand(TEXT("Startech.Test.Settings"),
		TEXT("Startech.Test.Settings [log|open|back|reset|drag <Volume> <Percent>]: the Settings page, with real mouse input."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Settings));
}

#endif
