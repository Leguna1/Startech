#include "GateMarkerSubsystem.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gates/JumpGate.h"
#include "MapMarkerComponent.h"
#include "StartechHUDLog.h"
#include "Tiers/StartechTierSubsystem.h"

namespace GateMarker
{
	// The runtime component's name, so the same component is found again on later sectors.
	const FName ComponentName(TEXT("HudGateMarker"));
}

void UGateMarkerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (USectorDirectorSubsystem* const Director = InWorld.GetSubsystem<USectorDirectorSubsystem>())
	{
		Director->OnSectorReady.AddUniqueDynamic(this, &UGateMarkerSubsystem::HandleSectorReady);
		BoundDirector = Director;
	}
}

void UGateMarkerSubsystem::Deinitialize()
{
	if (USectorDirectorSubsystem* const Director = BoundDirector.Get())
	{
		Director->OnSectorReady.RemoveDynamic(this, &UGateMarkerSubsystem::HandleSectorReady);
	}
	BoundDirector.Reset();

	Super::Deinitialize();
}

bool UGateMarkerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGateMarkerSubsystem::HandleSectorReady(const FName SectorId)
{
	int32 Registered = 0;
	for (TActorIterator<AJumpGate> It(GetWorld()); It; ++It)
	{
		AJumpGate& Gate = **It;

		// The pool hides gates it stores; they stay in the world but are not part of this sector.
		if (Gate.IsHidden())
		{
			if (UMapMarkerComponent* const Old = FindMarker(Gate))
			{
				Old->UnregisterMarker();
			}
			continue;
		}

		if (UMapMarkerComponent* const Marker = FindOrAddMarker(Gate))
		{
			Marker->Label = MakeLabel(Gate);
			ApplyTier(Gate, *Marker);
			Marker->RegisterMarker();
			++Registered;
		}
	}

	STARTECH_HUD_LOG(TEXT("Gate markers refreshed for sector %s: %d gate(s)"), *SectorId.ToString(), Registered);
}

UMapMarkerComponent* UGateMarkerSubsystem::FindMarker(const AJumpGate& Gate)
{
	TInlineComponentArray<UMapMarkerComponent*> Markers;
	Gate.GetComponents(Markers);
	UMapMarkerComponent* const* const Found = Markers.FindByPredicate([](const UMapMarkerComponent* const Marker)
	{
		return Marker->GetFName() == GateMarker::ComponentName;
	});
	return Found ? *Found : nullptr;
}

UMapMarkerComponent* UGateMarkerSubsystem::FindOrAddMarker(AJumpGate& Gate)
{
	if (UMapMarkerComponent* const Existing = FindMarker(Gate))
	{
		return Existing;
	}

	USceneComponent* const Root = Gate.GetRootComponent();
	if (!Root)
	{
		return nullptr;
	}

	UMapMarkerComponent* const Marker = NewObject<UMapMarkerComponent>(&Gate, GateMarker::ComponentName, RF_Transient);
	Marker->Type = EMapMarkerType::Gate;
	Marker->bShowOnRadar = true;
	Marker->bPinToRadarRim = true;
	Marker->bShowScreenMarker = true;
	Marker->bNearestOnly = false;
	Marker->SetupAttachment(Root);

	// The gate has begun play, so registering the component also runs its BeginPlay, which registers the marker.
	Gate.AddInstanceComponent(Marker);
	Marker->RegisterComponent();
	return Marker;
}

void UGateMarkerSubsystem::ApplyTier(const AJumpGate& Gate, UMapMarkerComponent& Marker) const
{
	const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(this);
	if (!Tiers)
	{
		Marker.LabelSuffix = FText::GetEmpty();
		return;
	}
	const FSectorTierData& Tier = Tiers->GetTierData(Tiers->GetTierForSector(Gate.GetDestinationSectorId()));
	Marker.LabelSuffix = FText::Format(NSLOCTEXT("GateMarker", "Tier", "Tier {0}"), Tier.DisplayName);
	Marker.LabelSuffixColor = Tier.DangerColor;
}

FText UGateMarkerSubsystem::MakeLabel(const AJumpGate& Gate) const
{
	const FName Destination = Gate.GetDestinationSectorId();
	const USectorDirectorSubsystem* const Director = BoundDirector.Get();
	const USectorCatalog* const Catalog = Director ? Director->GetCatalog() : nullptr;
	const FSectorDefinition* const Definition = Catalog ? Catalog->FindSector(Destination) : nullptr;
	const FText DestinationName = Definition ? Definition->DisplayName : FText::FromName(Destination);

	return FText::Format(NSLOCTEXT("GateMarker", "Label", "Gate · {0}"), DestinationName);
}
