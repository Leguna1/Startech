#include "Station.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "MapMarkerComponent.h"
#include "StartechShipPawn.h"

AStation::AStation()
{
	// Ticks only to repair a docked ship.
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	DockingZone = CreateDefaultSubobject<USphereComponent>(TEXT("DockingZone"));
	DockingZone->SetupAttachment(Root);
	DockingZone->InitSphereRadius(3000.f);
	DockingZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DockingZone->SetCollisionObjectType(ECC_WorldDynamic);
	DockingZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	DockingZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DockingZone->SetGenerateOverlapEvents(true);
	DockingZone->SetCanEverAffectNavigation(false);

	RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Root);

	MapMarker = CreateDefaultSubobject<UMapMarkerComponent>(TEXT("MapMarker"));
	MapMarker->SetupAttachment(Root);
	MapMarker->Type = EMapMarkerType::Station;
	MapMarker->Label = NSLOCTEXT("Station", "MarkerLabel", "Station");
	MapMarker->bPinToRadarRim = true;
	MapMarker->bShowScreenMarker = true;
	MapMarker->bNearestOnly = true;
}

void AStation::BeginPlay()
{
	Super::BeginPlay();

	// Bound here rather than in the constructor so the binding isn't saved into Blueprint defaults.
	DockingZone->OnComponentBeginOverlap.AddUniqueDynamic(this, &AStation::HandleBeginOverlap);
	DockingZone->OnComponentEndOverlap.AddUniqueDynamic(this, &AStation::HandleEndOverlap);

	// The marker registers itself on its own BeginPlay too; registration is idempotent.
	MapMarker->RegisterMarker();

	// A player already inside when play begins won't overlap again.
	DockOverlappingShip();
}

void AStation::OnAcquiredFromPool_Implementation()
{
	MapMarker->RegisterMarker();
	DockOverlappingShip();
}

void AStation::OnReleasedToPool_Implementation()
{
	Undock();
	MapMarker->UnregisterMarker();
}

FText AStation::GetDisplayName() const
{
	return MapMarker->Label.IsEmpty() ? NSLOCTEXT("Station", "DefaultName", "Station") : MapMarker->Label;
}

FTransform AStation::GetUndockTransform(const AActor& Ship) const
{
	const FVector Centre = DockingZone->GetComponentLocation();
	const FVector ShipLocation = Ship.GetActorLocation();

	// Flat, like the flight model; straight ahead of the station if the ship sits on its centre.
	FVector Away = FVector(ShipLocation.X - Centre.X, ShipLocation.Y - Centre.Y, 0.f).GetSafeNormal();
	if (Away.IsNearlyZero())
	{
		Away = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
	}
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}

	const float Distance = DockingZone->GetScaledSphereRadius() + Ship.GetSimpleCollisionRadius() + UndockClearance;
	const FVector Location(Centre.X + Away.X * Distance, Centre.Y + Away.Y * Distance, ShipLocation.Z);
	return FTransform(Away.Rotation(), Location);
}

void AStation::DockOverlappingShip()
{
	TArray<AActor*> Overlapping;
	DockingZone->GetOverlappingActors(Overlapping, AStartechShipPawn::StaticClass());
	if (AStartechShipPawn* const Ship = Overlapping.IsEmpty() ? nullptr : Cast<AStartechShipPawn>(Overlapping[0]))
	{
		Dock(*Ship);
	}
}

void AStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Undock();
	MapMarker->UnregisterMarker();

	Super::EndPlay(EndPlayReason);
}

void AStation::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (const AStartechShipPawn* const Ship = DockedShip.Get())
	{
		if (UHealthComponent* const Health = Ship->GetHealthComponent())
		{
			Health->RepairHull(RepairRate * DeltaSeconds);
		}
	}
}

FTransform AStation::GetRespawnTransform() const
{
	return RespawnPoint->GetComponentTransform();
}

AStation* AStation::FindNearestInPlay(const UWorld& World, const FVector& From)
{
	AStation* Nearest = nullptr;
	double NearestDistSq = TNumericLimits<double>::Max();

	for (TActorIterator<AStation> It(&World); It; ++It)
	{
		AStation* const Station = *It;
		// Hidden means stored in the pool: still a valid actor, but not in this sector.
		if (!IsValid(Station) || Station->IsHidden())
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(From, Station->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			Nearest = Station;
			NearestDistSq = DistSq;
		}
	}
	return Nearest;
}

void AStation::HandleBeginOverlap(UPrimitiveComponent* const OverlappedComponent, AActor* const OtherActor,
	UPrimitiveComponent* const OtherComp, const int32 OtherBodyIndex, const bool bFromSweep, const FHitResult& SweepResult)
{
	if (AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(OtherActor))
	{
		Dock(*Ship);
	}
}

void AStation::HandleEndOverlap(UPrimitiveComponent* const OverlappedComponent, AActor* const OtherActor,
	UPrimitiveComponent* const OtherComp, const int32 OtherBodyIndex)
{
	// An actor can overlap through more than one component; only undock once it has fully left.
	if (OtherActor && OtherActor == DockedShip.Get() && !DockingZone->IsOverlappingActor(OtherActor))
	{
		Undock();
	}
}

void AStation::Dock(AStartechShipPawn& Ship)
{
	if (DockedShip.Get() == &Ship)
	{
		return;
	}

	// One docked player at a time.
	Undock();

	DockedShip = &Ship;
	Ship.SetDockedStation(this);
}

void AStation::Undock()
{
	AStartechShipPawn* const Ship = DockedShip.Get();
	DockedShip = nullptr;

	if (Ship && Ship->GetDockedStation() == this)
	{
		Ship->SetDockedStation(nullptr);
	}
}
