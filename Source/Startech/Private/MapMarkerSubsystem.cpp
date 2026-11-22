#include "MapMarkerSubsystem.h"

#include "MapMarkerComponent.h"
#include "StartechHUDLog.h"

void UMapMarkerSubsystem::Register(UMapMarkerComponent* const Marker)
{
	if (!Marker || Contains(Marker))
	{
		return;
	}

	Markers.Add(Marker);
	STARTECH_HUD_LOG_VERBOSE(TEXT("OnMarkersChanged: registered %s on %s"), *Marker->Label.ToString(),
		*GetNameSafe(Marker->GetOwner()));
	OnMarkersChanged.Broadcast(Marker);
}

void UMapMarkerSubsystem::Unregister(UMapMarkerComponent* const Marker)
{
	// Also drops any entries whose component was destroyed without unregistering.
	const int32 Removed = Markers.RemoveAll([Marker](const TWeakObjectPtr<UMapMarkerComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Marker;
	});

	if (Removed > 0 && Marker)
	{
		STARTECH_HUD_LOG_VERBOSE(TEXT("OnMarkersChanged: unregistered %s on %s"), *Marker->Label.ToString(),
			*GetNameSafe(Marker->GetOwner()));
		OnMarkersChanged.Broadcast(Marker);
	}
}

void UMapMarkerSubsystem::NotifyMarkerChanged(UMapMarkerComponent* const Marker)
{
	if (!Marker || !Contains(Marker))
	{
		return;
	}

	STARTECH_HUD_LOG_VERBOSE(TEXT("OnMarkersChanged: %s on %s %s"), *Marker->Label.ToString(),
		*GetNameSafe(Marker->GetOwner()), Marker->IsMarkerEnabled() ? TEXT("enabled") : TEXT("disabled"));
	OnMarkersChanged.Broadcast(Marker);
}

bool UMapMarkerSubsystem::Contains(const UMapMarkerComponent* const Marker) const
{
	return Markers.ContainsByPredicate([Marker](const TWeakObjectPtr<UMapMarkerComponent>& Entry)
	{
		return Entry.Get() == Marker;
	});
}
