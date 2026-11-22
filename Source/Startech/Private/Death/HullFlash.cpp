#include "Death/HullFlash.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void FHullFlash::Start(UMeshComponent& Mesh, const TArray<FName>& ColorParameters, const FName EmissionParameter)
{
	Restore();

	const int32 NumSlots = Mesh.GetNumMaterials();
	if (NumSlots <= 0)
	{
		return;
	}

	FlashMesh = &Mesh;
	SavedOverrides = Mesh.OverrideMaterials;
	Emission = EmissionParameter;
	bChanged = true;

	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		UMaterialInstanceDynamic* const Material = Mesh.CreateDynamicMaterialInstance(Slot);
		if (!Material)
		{
			continue;
		}
		Materials.Add(Material);

		FSlot& Flashing = Slots.AddDefaulted_GetRef();
		Flashing.Material = Material;
		for (const FName Parameter : ColorParameters)
		{
			FLinearColor Value;
			if (!Parameter.IsNone() && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Value))
			{
				Flashing.Colors.Emplace(Parameter, Value);
			}
		}
		if (!EmissionParameter.IsNone())
		{
			Flashing.bHasEmission = Material->GetScalarParameterValue(FHashedMaterialParameterInfo(EmissionParameter),
				Flashing.Emission);
		}
	}
}

void FHullFlash::Apply(const FLinearColor& Tint, const float Amount, const float EmissionScale) const
{
	const float Blend = FMath::Clamp(Amount, 0.f, 1.f);
	for (const FSlot& Flashing : Slots)
	{
		UMaterialInstanceDynamic* const Material = Flashing.Material.Get();
		if (!Material)
		{
			continue;
		}
		for (const TPair<FName, FLinearColor>& Color : Flashing.Colors)
		{
			Material->SetVectorParameterValue(Color.Key, FMath::Lerp(Color.Value, Tint, Blend));
		}
		if (Flashing.bHasEmission)
		{
			// A base of 0 would never light up, so the boost starts from at least 1.
			Material->SetScalarParameterValue(Emission, FMath::Max(Flashing.Emission, 1.f) * EmissionScale);
		}
	}
}

void FHullFlash::Restore()
{
	if (bChanged)
	{
		if (UMeshComponent* const Mesh = FlashMesh.Get())
		{
			Mesh->EmptyOverrideMaterials();
			for (int32 Slot = 0; Slot < SavedOverrides.Num(); ++Slot)
			{
				if (SavedOverrides[Slot])
				{
					Mesh->SetMaterial(Slot, SavedOverrides[Slot]);
				}
			}
		}
	}

	bChanged = false;
	FlashMesh.Reset();
	SavedOverrides.Reset();
	Materials.Reset();
	Slots.Reset();
}
