#include "Data/SectorCatalog.h"
#include "Data/EnvironmentPreset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FSectorDefinition* USectorCatalog::FindSector(const FName SectorId) const
{
	return Sectors.FindByPredicate([SectorId](const FSectorDefinition& Sector)
	{
		return Sector.SectorId == SectorId;
	});
}

bool USectorCatalog::ContainsSector(const FName SectorId) const
{
	return FindSector(SectorId) != nullptr;
}

#if WITH_EDITOR
namespace
{
	/** A gate whose NormalizedLocation comes this close to the edge on any axis gets a validation warning. */
	constexpr double GateEdgeWarningMargin = 0.05;
}

EDataValidationResult USectorCatalog::IsDataValid(FDataValidationContext& Context) const
{
	bool bValid = Super::IsDataValid(Context) != EDataValidationResult::Invalid;

	for (int32 Index = 0; Index < Sectors.Num(); ++Index)
	{
		const FSectorDefinition& Sector = Sectors[Index];
		const FString Label = Sector.SectorId.ToString();

		if (Sector.SectorId.IsNone())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Sector at index %d has no SectorId."), Index)));
			bValid = false;
		}

		if (Sector.Preset == nullptr)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Sector '%s' has no Preset."), *Label)));
			bValid = false;
		}

		const bool bBodyKnown = StarSystem.Bodies.ContainsByPredicate([&Sector](const FCelestialBodyDefinition& Body)
		{
			return Body.BodyId == Sector.BodyId;
		});
		if (!Sector.BodyId.IsNone() && !bBodyKnown)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Sector '%s' refers to body '%s', which is not in the star system."), *Label, *Sector.BodyId.ToString())));
			bValid = false;
		}

		for (int32 Other = Index + 1; Other < Sectors.Num(); ++Other)
		{
			if (Sectors[Other].SectorId == Sector.SectorId)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("SectorId '%s' is used more than once."), *Label)));
				bValid = false;
			}
		}

		if (Sector.PlayAreaExtent.X <= 0.0 || Sector.PlayAreaExtent.Y <= 0.0 || Sector.PlayAreaExtent.Z <= 0.0)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Sector '%s': every component of PlayAreaExtent must be above 0 (is %s)."), *Label, *Sector.PlayAreaExtent.ToString())));
			bValid = false;
		}

		for (const FGateLink& Gate : Sector.Gates)
		{
			const FString GateLabel = Gate.DestinationSectorId.ToString();
			const double LargestAxis = Gate.NormalizedLocation.GetAbsMax();
			if (LargestAxis > 1.0)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Sector '%s': the gate to '%s' has a NormalizedLocation outside -1 to 1 (%s)."), *Label, *GateLabel, *Gate.NormalizedLocation.ToString())));
				bValid = false;
			}
			else if (LargestAxis > 1.0 - GateEdgeWarningMargin)
			{
				Context.AddWarning(FText::FromString(FString::Printf(
					TEXT("Sector '%s': the gate to '%s' is within %.2f of the play-area edge; arriving players will start in the bounds warning."), *Label, *GateLabel, GateEdgeWarningMargin)));
			}

			if (!ContainsSector(Gate.DestinationSectorId))
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Sector '%s' has a gate to unknown sector '%s'."), *Label, *Gate.DestinationSectorId.ToString())));
				bValid = false;
			}
			if (Gate.DestinationSectorId == Sector.SectorId)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("Sector '%s' has a gate to itself."), *Label)));
				bValid = false;
			}
		}
	}

	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
