#include "Population/SectorPlaceholderActors.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Population/RelevanceComponent.h"
#include "UObject/ConstructorHelpers.h"

ASectorPlaceholderActor::ASectorPlaceholderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	SetRootComponent(Mesh);

	Relevance = CreateDefaultSubobject<URelevanceComponent>(TEXT("Relevance"));
}

void ASectorPlaceholderActor::BeginPlay()
{
	Super::BeginPlay();

	ColorMID = Mesh->CreateDynamicMaterialInstance(0);
	if (ColorMID != nullptr)
	{
		ColorMID->SetVectorParameterValue(TEXT("Color"), PlaceholderColor);
	}
}

void ASectorPlaceholderActor::SetShape(const TCHAR* MeshPath, const FVector& Scale)
{
	ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(MeshPath);
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(Scale);
}

ASectorPlaceholderEnemy::ASectorPlaceholderEnemy()
{
	SetShape(TEXT("/Engine/BasicShapes/Cone.Cone"), FVector(6.0));
	PlaceholderColor = FLinearColor(0.9f, 0.1f, 0.1f);
}

ASectorPlaceholderLoot::ASectorPlaceholderLoot()
{
	SetShape(TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(3.0));
	PlaceholderColor = FLinearColor(1.0f, 0.8f, 0.1f);
}

ASectorPlaceholderStation::ASectorPlaceholderStation()
{
	SetShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(80.0, 80.0, 25.0));
	PlaceholderColor = FLinearColor(0.5f, 0.5f, 0.55f);
}
