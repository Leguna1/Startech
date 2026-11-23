#pragma once

#include "CoreMinimal.h"
#include "Audio/StartechAudioTypes.h"
#include "UI/Menu/MenuPageWidget.h"
#include "SettingsScreenWidget.generated.h"

class UHubButtonWidget;
class UHubSliderRowWidget;

/**
 * The Settings page, a full-screen page (UMenuPageWidget) over the main menu and the pause menu: one slider row
 * (UHubSliderRowWidget) per volume, Reset to defaults and Back.
 * - The rows are Master, Music, Menu music (main menu only: SetShowMenuMusic), Weapons, Explosions.
 * - Moving a slider applies it at once (UStartechAudioSubsystem::SetVolumes), so the music is heard changing; letting
 *   go of Weapons or Explosions plays one sample of that group.
 * - Reset to defaults applies UStartechSettings::Audio.DefaultVolumes.
 * - Closing the page (Back, Escape) saves the volumes to the machine's user settings.
 */
UCLASS()
class STARTECH_API USettingsScreenWidget : public UMenuPageWidget
{
	GENERATED_BODY()

public:
	/** Shows or hides the Menu music row (the main menu shows it; the pause menu doesn't). */
	void SetShowMenuMusic(bool bShow);

	/** True when the Menu music row is shown. */
	bool IsMenuMusicShown() const;

	/** The slider for Volume, for tests. */
	UHubSliderRowWidget* GetSlider(EStartechVolume Volume) const;

	/** Every row in screen order (hidden ones included), for tests. */
	const TArray<TObjectPtr<UHubSliderRowWidget>>& GetSliders() const { return Sliders; }

	UHubButtonWidget* GetResetButton() const { return ResetButton; }
	UHubButtonWidget* GetBackButton() const { return BackButton; }

	/** Reset to defaults: what the button does. */
	void ResetToDefaults();

protected:
	/** The panel with the title, the rows and the buttons. */
	virtual void BuildContent() override;

	/** Binds the rows and buttons. */
	virtual void NativeConstruct() override;

	/** Unbinds them. */
	virtual void NativeDestruct() override;

	/** Fills the sliders from the volumes now in use. */
	virtual void OnPageOpened() override;

	/** Saves the volumes. */
	virtual void OnPageClosing() override;

private:
	/** A slider moved: applies the volumes at once. */
	void HandleChanged(float Value, EStartechVolume Volume);

	/** A slider was let go: a sample for Weapons and Explosions. */
	void HandleCommitted(float Value, EStartechVolume Volume);

	/** Sets every slider from Volumes. */
	void ShowVolumes(const FStartechVolumes& Volumes);

	/** One per EStartechVolume, in its order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHubSliderRowWidget>> Sliders;

	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> ResetButton;

	UPROPERTY(Transient)
	TObjectPtr<UHubButtonWidget> BackButton;
};
