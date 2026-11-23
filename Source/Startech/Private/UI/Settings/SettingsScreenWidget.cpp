#include "UI/Settings/SettingsScreenWidget.h"

#include "Audio/StartechAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "StartechSettings.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/HubPanelWidget.h"
#include "UI/Hub/HubSliderRowWidget.h"

#define LOCTEXT_NAMESPACE "SettingsScreen"

namespace SettingsScreen
{
	constexpr float Width = 600.f;
	constexpr float Inset = 28.f;
	constexpr int32 TitleSize = 22;
	constexpr float TitleGap = 16.f;
	constexpr float HeadingGap = 6.f;
	constexpr float RowGap = 4.f;
	constexpr float ButtonsGap = 22.f;
	constexpr float ButtonWidth = 180.f;
	constexpr float ButtonSpacing = 16.f;

	static const EStartechVolume Order[] = { EStartechVolume::Master, EStartechVolume::Music, EStartechVolume::MenuMusic,
		EStartechVolume::Weapons, EStartechVolume::Explosions };

	static FText Label(const EStartechVolume Volume)
	{
		switch (Volume)
		{
		case EStartechVolume::Music: return LOCTEXT("Music", "Music");
		case EStartechVolume::MenuMusic: return LOCTEXT("MenuMusic", "Menu music");
		case EStartechVolume::Weapons: return LOCTEXT("Weapons", "Weapons");
		case EStartechVolume::Explosions: return LOCTEXT("Explosions", "Explosions");
		default: return LOCTEXT("Master", "Master");
		}
	}
}

void USettingsScreenWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	USizeBox* const Frame = WidgetTree->ConstructWidget<USizeBox>();
	Frame->SetWidthOverride(SettingsScreen::Width);
	UOverlaySlot* const FrameSlot = Root->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Center);
	FrameSlot->SetVerticalAlignment(VAlign_Center);

	UOverlay* const Card = WidgetTree->ConstructWidget<UOverlay>();
	Frame->AddChild(Card);
	UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
	UOverlaySlot* const PanelSlot = Card->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Fill);
	PanelSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* const ColumnSlot = Card->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetPadding(FMargin(SettingsScreen::Inset));

	UTextBlock* const Title = MakeText(LOCTEXT("Title", "Settings"), SettingsScreen::TitleSize, HudStyle.PlayerColor, true);
	UVerticalBoxSlot* const TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, SettingsScreen::TitleGap));

	UTextBlock* const Heading = MakeText(LOCTEXT("Audio", "Audio"), HudStyle.HeadingFontSize, HudStyle.PlayerColor, true);
	Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.f, 0.f, 0.f, SettingsScreen::HeadingGap));

	for (const EStartechVolume Volume : SettingsScreen::Order)
	{
		UHubSliderRowWidget* const Slider = WidgetTree->ConstructWidget<UHubSliderRowWidget>(UHubSliderRowWidget::StaticClass());
		Slider->SetLabel(SettingsScreen::Label(Volume));
		Column->AddChildToVerticalBox(Slider)->SetPadding(FMargin(0.f, SettingsScreen::RowGap));
		Sliders.Add(Slider);
	}

	UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* const ButtonsSlot = Column->AddChildToVerticalBox(Buttons);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonsSlot->SetPadding(FMargin(0.f, SettingsScreen::ButtonsGap, 0.f, 0.f));

	ResetButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	ResetButton->SetLabel(LOCTEXT("Reset", "Reset to defaults"));
	ResetButton->SetVariant(EHubButtonVariant::Secondary);
	ResetButton->SetFixedWidth(SettingsScreen::ButtonWidth);
	Buttons->AddChildToHorizontalBox(ResetButton)->SetPadding(FMargin(0.f, 0.f, SettingsScreen::ButtonSpacing, 0.f));

	BackButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	BackButton->SetLabel(LOCTEXT("Back", "Back"));
	BackButton->SetFixedWidth(SettingsScreen::ButtonWidth);
	Buttons->AddChildToHorizontalBox(BackButton);
}

void USettingsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		const EStartechVolume Volume = SettingsScreen::Order[Index];
		Sliders[Index]->OnValueChanged.AddUObject(this, &USettingsScreenWidget::HandleChanged, Volume);
		Sliders[Index]->OnValueCommitted.AddUObject(this, &USettingsScreenWidget::HandleCommitted, Volume);
	}
	ResetButton->OnClicked.AddUObject(this, &USettingsScreenWidget::ResetToDefaults);
	BackButton->OnClicked.AddUObject(this, &USettingsScreenWidget::Close);
}

void USettingsScreenWidget::NativeDestruct()
{
	for (UHubSliderRowWidget* const Slider : Sliders)
	{
		Slider->OnValueChanged.RemoveAll(this);
		Slider->OnValueCommitted.RemoveAll(this);
	}
	ResetButton->OnClicked.RemoveAll(this);
	BackButton->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void USettingsScreenWidget::SetShowMenuMusic(const bool bShow)
{
	if (UHubSliderRowWidget* const Slider = GetSlider(EStartechVolume::MenuMusic))
	{
		Slider->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

bool USettingsScreenWidget::IsMenuMusicShown() const
{
	const UHubSliderRowWidget* const Slider = GetSlider(EStartechVolume::MenuMusic);
	return Slider && Slider->GetVisibility() != ESlateVisibility::Collapsed;
}

UHubSliderRowWidget* USettingsScreenWidget::GetSlider(const EStartechVolume Volume) const
{
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		if (SettingsScreen::Order[Index] == Volume)
		{
			return Sliders[Index];
		}
	}
	return nullptr;
}

void USettingsScreenWidget::ShowVolumes(const FStartechVolumes& Volumes)
{
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		Sliders[Index]->SetValue(Volumes.Get(SettingsScreen::Order[Index]));
	}
}

void USettingsScreenWidget::OnPageOpened()
{
	const UStartechAudioSubsystem* const Audio = IsDesignTime() ? nullptr : UStartechAudioSubsystem::Get(this);
	ShowVolumes(Audio ? Audio->GetVolumes() : UStartechAudioSubsystem::LoadSavedVolumes());
}

void USettingsScreenWidget::OnPageClosing()
{
	if (const UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		Audio->SaveVolumes();
	}
}

void USettingsScreenWidget::HandleChanged(const float Value, const EStartechVolume Volume)
{
	if (UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		FStartechVolumes Volumes = Audio->GetVolumes();
		Volumes.Set(Volume, Value);
		Audio->SetVolumes(Volumes);
	}
}

void USettingsScreenWidget::HandleCommitted(const float Value, const EStartechVolume Volume)
{
	if (const UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		Audio->PlayPreview(Volume);
	}
}

void USettingsScreenWidget::ResetToDefaults()
{
	const FStartechVolumes& Defaults = UStartechSettings::Get().Audio.DefaultVolumes;
	if (UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		Audio->SetVolumes(Defaults);
	}
	ShowVolumes(Defaults);
}

#undef LOCTEXT_NAMESPACE
