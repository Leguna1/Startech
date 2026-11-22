#include "Gates/JumpGate.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Gates/WarpTransitionComponent.h"
#include "UObject/ConstructorHelpers.h"

AJumpGate::AJumpGate()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetMobility(EComponentMobility::Movable);
	Trigger->InitSphereRadius(2000.0f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(Trigger);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GateMesh->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	GateMesh->SetRelativeScale3D(FVector(40.0, 40.0, 1.0));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0.0, 0.0, 2600.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(900.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		GateMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void AJumpGate::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AJumpGate::HandleTriggerBeginOverlap);
}

void AJumpGate::Configure(const FGateLink& Link, const FVector& WorldLocation, const FText& DestinationLabel)
{
	DestinationSectorId = Link.DestinationSectorId;
	Label->SetText(DestinationLabel);

	// Face the level centre so the label reads correctly for a ship approaching from inside.
	const FVector ToCentre = (-WorldLocation).GetSafeNormal();
	const FRotator Facing = ToCentre.IsNearlyZero() ? FRotator::ZeroRotator : ToCentre.Rotation();
	SetActorLocationAndRotation(WorldLocation, Facing, false, nullptr, ETeleportType::TeleportPhysics);
}

FName AJumpGate::GetDestinationSectorId() const
{
	return DestinationSectorId;
}

void AJumpGate::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor == nullptr || DestinationSectorId.IsNone())
	{
		return;
	}

	if (UWarpTransitionComponent* Warp = OtherActor->FindComponentByClass<UWarpTransitionComponent>())
	{
		// Retried while the pawn stays inside if the game refuses it (for example, in combat).
		Warp->BeginWarpFromTrigger(DestinationSectorId, this);
	}
}
