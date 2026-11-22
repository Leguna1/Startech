#include "Environment/AsteroidField.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Data/PlayAreaMath.h"
#include "Data/ProcSectorSettings.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProcSectorLog.h"
#include "Templates/TypeHash.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** True when the transform carries the tiny scale removed asteroids are given. */
	bool IsRemovedTransform(const FTransform& Transform, const float RemovedScale)
	{
		return Transform.GetScale3D().GetMax() <= RemovedScale * 2.0f;
	}
}

AAsteroidField::AAsteroidField()
{
	PrimaryActorTick.bCanEverTick = false;

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionProfileName(TEXT("BlockAll"));
	Instances->SetCastShadow(false);
	SetRootComponent(Instances);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Instances->SetStaticMesh(CubeMesh.Object);
	}
}

void AAsteroidField::BeginFill(const FAsteroidFieldParams& Params, const FVector& PlayAreaExtent, const int32 Seed, const TArray<int32>& RemovedIds)
{
	ClearField();

	ActiveParams = Params;
	ActivePlayAreaExtent = PlayAreaExtent;
	PendingRemovedIds = RemovedIds;
	Stream.Initialize(Seed);
	TypeStream.Initialize(static_cast<int32>(HashCombineFast(static_cast<uint32>(Seed), TypeSeedSalt)));
	bFilling = Params.Count > 0;

	const int32 TypeCount = Params.Types.Num();
	TotalTypeWeight = 0.0f;
	for (const FAsteroidTypeParams& Type : Params.Types)
	{
		TotalTypeWeight += FMath::Max(Type.Weight, 0.0f);
	}

	EnsureTypeComponents(TypeCount);
	TypeInstanceIds.SetNum(TypeCount);
	for (int32 TypeIndex = 0; TypeIndex < TypeComponents.Num(); ++TypeIndex)
	{
		if (TypeIndex < TypeCount)
		{
			SetUpTypeComponent(TypeIndex, Params.Types[TypeIndex]);
		}
		else
		{
			TypeComponents[TypeIndex]->SetVisibility(false);
		}
	}

	// The root keeps its own visibility separate from its children, so hiding it leaves the type components visible.
	Instances->SetVisibility(TypeCount == 0);

	if (TintMID == nullptr)
	{
		TintMID = Instances->CreateDynamicMaterialInstance(0);
	}
	if (TintMID != nullptr)
	{
		TintMID->SetVectorParameterValue(TintParameter, Params.Tint);
	}
}

bool AAsteroidField::FillSlice(const double DeadlineSeconds)
{
	if (!bFilling)
	{
		return true;
	}

	const int32 BatchSize = FMath::Max(UProcSectorSettings::Get().AsteroidsPerBatch, 1);
	TArray<FTransform> Batch;
	Batch.Reserve(BatchSize);

	// One batch per type component, filled side by side so ids keep counting across all types.
	TArray<TArray<FTransform>> TypeBatches;
	TypeBatches.SetNum(ActiveParams.Types.Num());

	while (NextAsteroidId < ActiveParams.Count)
	{
		Batch.Reset();
		for (TArray<FTransform>& TypeBatch : TypeBatches)
		{
			TypeBatch.Reset();
		}

		const int32 BatchEnd = FMath::Min(NextAsteroidId + BatchSize, ActiveParams.Count);
		while (NextAsteroidId < BatchEnd)
		{
			int32 TypeIndex = INDEX_NONE;
			FTransform Transform = MakeNextTransform(TypeIndex);
			if (PendingRemovedIds.Contains(NextAsteroidId))
			{
				Transform.SetScale3D(FVector(RemovedScale));
			}

			FAsteroidSlot& Slot = Slots.AddDefaulted_GetRef();
			Slot.TypeIndex = TypeIndex;
			if (TypeIndex == INDEX_NONE)
			{
				// Without types every asteroid is in Instances, so the instance index is the id.
				Slot.InstanceIndex = NextAsteroidId;
				Batch.Add(Transform);
			}
			else
			{
				// Components only ever grow during a fill, so the next instance index is the number added so far.
				Slot.InstanceIndex = TypeInstanceIds[TypeIndex].Add(NextAsteroidId);
				TypeBatches[TypeIndex].Add(Transform);
			}
			++NextAsteroidId;
		}

		// Transforms are in the component's local space; the field actor normally sits at the origin.
		if (!Batch.IsEmpty())
		{
			Instances->AddInstances(Batch, false, false);
		}
		for (int32 TypeIndex = 0; TypeIndex < TypeBatches.Num(); ++TypeIndex)
		{
			if (!TypeBatches[TypeIndex].IsEmpty())
			{
				TypeComponents[TypeIndex]->AddInstances(TypeBatches[TypeIndex], false, false);
			}
		}

		if (FPlatformTime::Seconds() >= DeadlineSeconds)
		{
			break;
		}
	}

	if (NextAsteroidId >= ActiveParams.Count)
	{
		bFilling = false;
		PendingRemovedIds.Reset();
	}

	return !bFilling;
}

void AAsteroidField::ClearField()
{
	Instances->ClearInstances();
	for (UInstancedStaticMeshComponent* const Component : TypeComponents)
	{
		if (Component != nullptr)
		{
			Component->ClearInstances();
		}
	}

	Slots.Reset();
	TypeInstanceIds.Reset();
	NextAsteroidId = 0;
	bFilling = false;
	PendingRemovedIds.Reset();
}

bool AAsteroidField::RemoveAsteroid(const int32 AsteroidId)
{
	if (!Slots.IsValidIndex(AsteroidId))
	{
		return false;
	}

	const FAsteroidSlot& Slot = Slots[AsteroidId];
	UInstancedStaticMeshComponent* const Component = GetComponentForType(Slot.TypeIndex);

	FTransform Transform;
	if (Component == nullptr || !Component->GetInstanceTransform(Slot.InstanceIndex, Transform, false))
	{
		return false;
	}

	if (IsRemovedTransform(Transform, RemovedScale))
	{
		return false;
	}

	Transform.SetScale3D(FVector(RemovedScale));
	Component->UpdateInstanceTransform(Slot.InstanceIndex, Transform, false, true, true);

	OnAsteroidRemoved.Broadcast(AsteroidId);
	return true;
}

int32 AAsteroidField::GetAsteroidIdFromHit(const FHitResult& Hit) const
{
	const UPrimitiveComponent* const HitComponent = Hit.GetComponent();
	if (HitComponent == nullptr)
	{
		return INDEX_NONE;
	}

	if (HitComponent == Instances)
	{
		const bool bUntyped = Slots.IsValidIndex(Hit.Item) && Slots[Hit.Item].TypeIndex == INDEX_NONE;
		return bUntyped ? Hit.Item : INDEX_NONE;
	}

	const int32 TypeIndex = TypeComponents.IndexOfByPredicate([HitComponent](const TObjectPtr<UInstancedStaticMeshComponent>& Component)
	{
		return Component.Get() == HitComponent;
	});
	if (!TypeInstanceIds.IsValidIndex(TypeIndex))
	{
		return INDEX_NONE;
	}

	const TArray<int32>& InstanceIds = TypeInstanceIds[TypeIndex];
	return InstanceIds.IsValidIndex(Hit.Item) ? InstanceIds[Hit.Item] : INDEX_NONE;
}

bool AAsteroidField::GetAsteroidInfo(const int32 AsteroidId, int32& OutTypeIndex, FTransform& OutTransform) const
{
	if (!Slots.IsValidIndex(AsteroidId))
	{
		return false;
	}

	const FAsteroidSlot& Slot = Slots[AsteroidId];
	const UInstancedStaticMeshComponent* const Component = GetComponentForType(Slot.TypeIndex);

	FTransform LocalTransform;
	if (Component == nullptr || !Component->GetInstanceTransform(Slot.InstanceIndex, LocalTransform, false) || IsRemovedTransform(LocalTransform, RemovedScale))
	{
		return false;
	}

	FTransform WorldTransform;
	Component->GetInstanceTransform(Slot.InstanceIndex, WorldTransform, true);

	OutTypeIndex = Slot.TypeIndex;
	OutTransform = WorldTransform;
	return true;
}

int32 AAsteroidField::GetAsteroidCount() const
{
	return Slots.Num();
}

bool AAsteroidField::IsFilling() const
{
	return bFilling;
}

FTransform AAsteroidField::MakeNextTransform(int32& OutTypeIndex)
{
	FVector Location = FVector::ZeroVector;
	switch (ActiveParams.Distribution)
	{
	case EAsteroidDistribution::Sphere:
		Location = MakeSphereLocation();
		break;
	case EAsteroidDistribution::Box:
		Location = MakeBoxLocation();
		break;
	case EAsteroidDistribution::Belt:
	default:
		Location = FPlayAreaMath::RandomPointInRing(Stream, ActiveParams.InnerFraction, ActiveParams.OuterFraction, ActiveParams.HeightFraction, ActivePlayAreaExtent);
		break;
	}

	const FRotator Rotation(Stream.FRandRange(0.0f, 360.0f), Stream.FRandRange(0.0f, 360.0f), Stream.FRandRange(0.0f, 360.0f));

	if (ActiveParams.Types.IsEmpty())
	{
		// The original single-mesh draws, untouched, so fields without types keep their exact layout.
		OutTypeIndex = INDEX_NONE;
		const float MaxScale = FMath::Max(ActiveParams.MaxScale, ActiveParams.MinScale);
		const FVector Scale(
			Stream.FRandRange(ActiveParams.MinScale, MaxScale),
			Stream.FRandRange(ActiveParams.MinScale, MaxScale),
			Stream.FRandRange(ActiveParams.MinScale, MaxScale));

		return FTransform(Rotation, Location, Scale);
	}

	// The same three scale draws as above, mapped onto the type's range; the type itself comes from its own stream.
	const float AlphaX = Stream.FRand();
	const float AlphaY = Stream.FRand();
	const float AlphaZ = Stream.FRand();
	OutTypeIndex = PickTypeIndex();

	const FAsteroidTypeParams& Type = ActiveParams.Types[OutTypeIndex];
	const float TypeMaxScale = FMath::Max(Type.MaxScale, Type.MinScale);
	const FVector Scale(
		FMath::Lerp(Type.MinScale, TypeMaxScale, AlphaX),
		FMath::Lerp(Type.MinScale, TypeMaxScale, AlphaY),
		FMath::Lerp(Type.MinScale, TypeMaxScale, AlphaZ));

	return FTransform(Rotation, Location, Scale);
}

int32 AAsteroidField::PickTypeIndex()
{
	const int32 TypeCount = ActiveParams.Types.Num();
	const float Draw = TypeStream.FRand();

	if (TotalTypeWeight <= 0.0f)
	{
		// Invalid weights (validation reports them): share evenly rather than drop asteroids.
		return FMath::Min(FMath::FloorToInt32(Draw * TypeCount), TypeCount - 1);
	}

	float Remaining = Draw * TotalTypeWeight;
	int32 LastWeighted = 0;
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		const float Weight = FMath::Max(ActiveParams.Types[TypeIndex].Weight, 0.0f);
		if (Weight <= 0.0f)
		{
			continue;
		}
		if (Remaining < Weight)
		{
			return TypeIndex;
		}
		Remaining -= Weight;
		LastWeighted = TypeIndex;
	}

	// Float rounding can leave a sliver past the last weight.
	return LastWeighted;
}

void AAsteroidField::EnsureTypeComponents(const int32 Count)
{
	while (TypeComponents.Num() < Count)
	{
		UInstancedStaticMeshComponent* const Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(Instances->GetCollisionEnabled());
		Component->SetCollisionObjectType(Instances->GetCollisionObjectType());
		Component->SetCollisionResponseToChannels(Instances->GetCollisionResponseToChannels());
		Component->SetGenerateOverlapEvents(Instances->GetGenerateOverlapEvents());
		Component->SetCastShadow(Instances->CastShadow);
		Component->SetupAttachment(Instances);
		Component->RegisterComponent();

		TypeComponents.Add(Component);
		TypeMIDs.Add(nullptr);
	}
}

void AAsteroidField::SetUpTypeComponent(const int32 TypeIndex, const FAsteroidTypeParams& Type)
{
	UInstancedStaticMeshComponent* const Component = TypeComponents[TypeIndex];

	// Runs during the warp, behind the fade, so a short synchronous load is not seen.
	UStaticMesh* const Mesh = Type.Mesh.LoadSynchronous();
	if (Mesh == nullptr)
	{
		UE_LOG(LogProcSector, Warning, TEXT("Asteroid type %d has no mesh, or it could not be loaded; its asteroids keep their ids but are not drawn."), TypeIndex);
	}

	Component->SetStaticMesh(Mesh);
	Component->EmptyOverrideMaterials();

	UMaterialInterface* const OverrideMaterial = Type.Material.LoadSynchronous();
	UMaterialInterface* const BaseMaterial = OverrideMaterial != nullptr ? OverrideMaterial : (Mesh != nullptr ? Mesh->GetMaterial(0) : nullptr);

	UMaterialInstanceDynamic* MID = TypeMIDs[TypeIndex];
	if (BaseMaterial == nullptr)
	{
		MID = nullptr;
	}
	else if (MID == nullptr || MID->Parent != BaseMaterial)
	{
		MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}
	TypeMIDs[TypeIndex] = MID;

	if (MID != nullptr)
	{
		Component->SetMaterial(0, MID);
		MID->SetVectorParameterValue(TintParameter, Type.Tint);
	}

	Component->SetVisibility(true);
}

UInstancedStaticMeshComponent* AAsteroidField::GetComponentForType(const int32 TypeIndex) const
{
	if (TypeIndex == INDEX_NONE)
	{
		return Instances;
	}
	return TypeComponents.IsValidIndex(TypeIndex) ? TypeComponents[TypeIndex].Get() : nullptr;
}

FVector AAsteroidField::MakeSphereLocation()
{
	const double Inner = FMath::Clamp(ActiveParams.InnerFraction, 0.0f, 1.0f);
	const double Outer = FMath::Max(static_cast<double>(FMath::Clamp(ActiveParams.OuterFraction, 0.0f, 1.0f)), Inner);
	const double Angle = Stream.FRandRange(0.0f, UE_TWO_PI);
	const double Blend = Stream.FRand();

	// Cube root keeps the density even through the volume; a uniform height picks an even direction on the sphere.
	const double Radius = FMath::Pow(FMath::Lerp(Inner * Inner * Inner, Outer * Outer * Outer, Blend), 1.0 / 3.0);
	const double UnitHeight = Stream.FRandRange(-1.0f, 1.0f);
	const double Flat = FMath::Sqrt(FMath::Max(0.0, 1.0 - UnitHeight * UnitHeight));
	const FVector Unit = FVector(Flat * FMath::Cos(Angle), Flat * FMath::Sin(Angle), UnitHeight) * Radius;

	return FPlayAreaMath::ToWorld(Unit, ActivePlayAreaExtent);
}

FVector AAsteroidField::MakeBoxLocation()
{
	const double Inner = FMath::Clamp(ActiveParams.InnerFraction, 0.0f, 1.0f);
	const double Outer = FMath::Clamp(ActiveParams.OuterFraction, 0.0f, 1.0f);
	const double Height = FMath::Clamp(ActiveParams.HeightFraction, 0.0f, 1.0f);

	// Always three draws per attempt in X, Y, Z order, and a fixed redraw limit, so the stream advances the same way for a given seed.
	FVector Unit = FVector::ZeroVector;
	for (int32 Attempt = 0; Attempt <= MaxBoxRedraws; ++Attempt)
	{
		const double X = Stream.FRandRange(-1.0f, 1.0f) * Outer;
		const double Y = Stream.FRandRange(-1.0f, 1.0f) * Outer;
		const double Z = Stream.FRandRange(-1.0f, 1.0f) * Height;
		Unit = FVector(X, Y, Z);

		if (X * X + Y * Y >= Inner * Inner)
		{
			break;
		}
	}

	return FPlayAreaMath::ToWorld(Unit, ActivePlayAreaExtent);
}
