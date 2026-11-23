#include "Audio/StartechUserSettings.h"

#include "Engine/Engine.h"
#include "StartechSettings.h"

UStartechUserSettings* UStartechUserSettings::Get()
{
	return GEngine ? Cast<UStartechUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

FStartechVolumes UStartechUserSettings::GetVolumes() const
{
	return bHasSavedVolumes ? Volumes : UStartechSettings::Get().Audio.DefaultVolumes;
}

void UStartechUserSettings::ClearSavedVolumes()
{
	Volumes = FStartechVolumes();
	bHasSavedVolumes = false;
}

void UStartechUserSettings::SetVolumes(const FStartechVolumes& InVolumes)
{
	Volumes = InVolumes;
	bHasSavedVolumes = true;
}
