#include "ModularShipComponent.h"
#include "Startech/Public/ModularShipComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

const FString UModularShipComponent::PartPrefix = TEXT("ShipPart_");

UModularShipComponent::UModularShipComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<FShipPartSpec> UModularShipComponent::GetPartSpecs() const
{
	TArray<FShipPartSpec> Specs;
	if (Loadout)
	{
		Specs = Loadout->Parts;
	}
	for (const FShipPartSpec& Extra : ExtraParts)
	{
		if (FShipPartSpec* Existing = Specs.FindByPredicate([&](const FShipPartSpec& S) { return S.Id == Extra.Id; }))
		{
			*Existing = Extra;
		}
		else
		{
			Specs.Add(Extra);
		}
	}
	return Specs;
}

void UModularShipComponent::ClearBuilt()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	// Also catches parts made by an earlier construction-script run that this instance doesn't know about.
	TArray<UStaticMeshComponent*> Comps;
	Owner->GetComponents<UStaticMeshComponent>(Comps);
	for (UStaticMeshComponent* C : Comps)
	{
		if (C && C->GetName().StartsWith(PartPrefix) && C->GetAttachmentRoot() == GetAttachmentRoot())
		{
			C->DestroyComponent();
		}
	}
	Built.Reset();
}

void UModularShipComponent::Rebuild()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	ClearBuilt();

	UMaterialInterface* PaintMat = Paint ? Paint.Get() : (Loadout ? Loadout->Paint.Get() : nullptr);
	TArray<FShipPartSpec> Pending = GetPartSpecs();

	// Build parents before children, whatever order the list is in.
	bool bProgress = true;
	while (Pending.Num() > 0 && bProgress)
	{
		bProgress = false;
		for (int32 i = 0; i < Pending.Num(); ++i)
		{
			const FShipPartSpec Spec = Pending[i];
			USceneComponent* ParentComp = this;
			if (!Spec.Parent.IsNone())
			{
				const TObjectPtr<UStaticMeshComponent>* P = Built.Find(Spec.Parent);
				if (!P)
				{
					continue;   // parent not built yet
				}
				ParentComp = P->Get();
			}
			Pending.RemoveAt(i--);
			bProgress = true;
			if (!Spec.Mesh || Spec.Id.IsNone())
			{
				continue;
			}
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(
				Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), FName(*(PartPrefix + Spec.Id.ToString()))));
			C->CreationMethod = Owner->IsRunningUserConstructionScript() ? EComponentCreationMethod::UserConstructionScript
			                                                             : EComponentCreationMethod::Instance;
			C->SetStaticMesh(Spec.Mesh);
			C->SetCollisionProfileName(PartCollisionProfile);
			if (PaintMat)
			{
				C->SetMaterial(0, PaintMat);
			}
			if (!Spec.Parent.IsNone() && !Spec.Socket.IsNone() && !ParentComp->DoesSocketExist(Spec.Socket))
			{
				UE_LOG(LogTemp, Warning, TEXT("Ship: part %s - socket %s not found on %s"), *Spec.Id.ToString(),
				       *Spec.Socket.ToString(), *Spec.Parent.ToString());
			}
			C->SetupAttachment(ParentComp, Spec.Socket);
			C->RegisterComponent();
			Built.Add(Spec.Id, C);
		}
	}
	for (const FShipPartSpec& Orphan : Pending)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ship: part %s skipped - its parent %s is missing"), *Orphan.Id.ToString(),
		       *Orphan.Parent.ToString());
	}
}

void UModularShipComponent::SetPart(FName Id, UStaticMesh* Mesh, FName Parent, FName Socket)
{
	FShipPartSpec Spec;
	Spec.Id = Id;
	Spec.Mesh = Mesh;
	Spec.Parent = Parent;
	Spec.Socket = Socket;
	if (FShipPartSpec* Existing = ExtraParts.FindByPredicate([&](const FShipPartSpec& S) { return S.Id == Id; }))
	{
		*Existing = Spec;
	}
	else
	{
		ExtraParts.Add(Spec);
	}
	Rebuild();
}

void UModularShipComponent::RemovePart(FName Id)
{
	// A part with no mesh is skipped by Rebuild, and so is everything plugged into it.
	SetPart(Id, nullptr, NAME_None, NAME_None);
}

UStaticMeshComponent* UModularShipComponent::GetPart(FName Id) const
{
	const TObjectPtr<UStaticMeshComponent>* P = Built.Find(Id);
	return P ? P->Get() : nullptr;
}

bool UModularShipComponent::AttachToPilotEye(USceneComponent* Component)
{
	UStaticMeshComponent* Part = GetPart(PilotEyePart);
	if (!Component || !Part || !Part->DoesSocketExist(PilotEyeSocket))
	{
		return false;
	}
	return Component->AttachToComponent(Part, FAttachmentTransformRules::SnapToTargetNotIncludingScale, PilotEyeSocket);
}

void UModularShipComponent::GetMuzzles(TArray<FTransform>& Transforms, TArray<FName>& PartIds) const
{
	static const FName Muzzle(TEXT("Muzzle"));
	Transforms.Reset();
	PartIds.Reset();
	for (const TPair<FName, TObjectPtr<UStaticMeshComponent>>& It : Built)
	{
		if (It.Value && It.Value->DoesSocketExist(Muzzle))
		{
			Transforms.Add(It.Value->GetSocketTransform(Muzzle));
			PartIds.Add(It.Key);
		}
	}
}

void UModularShipComponent::BeginPlay()
{
	Super::BeginPlay();
	// Parts made by the construction script are not in Built after a load: rebuild once so the map is filled.
	Rebuild();
}

#if WITH_EDITOR
void UModularShipComponent::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	if (GetOwner() && !GetOwner()->IsTemplate())
	{
		Rebuild();
	}
}
#endif

AModularShipActor::AModularShipActor()
{
	Ship = CreateDefaultSubobject<UModularShipComponent>(TEXT("Ship"));
	RootComponent = Ship;
}

void AModularShipActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Ship)
	{
		Ship->Rebuild();
	}
}
