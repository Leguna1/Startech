#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/SectorTypes.h"
#include "SectorCatalog.generated.h"

/**
 * The list of every sector in the game and how their gates connect.
 * Assign one in Project Settings > Plugins > ProcSector. If none is assigned,
 * FDefaultCatalogFactory builds a four-sector test catalog in code.
 */
UCLASS(BlueprintType)
class PROCSECTOR_API USectorCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/** All sectors. Stored as an array and searched linearly; catalogs are small. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	TArray<FSectorDefinition> Sectors;

	/** The sun and planets all sectors belong to. Leave Bodies empty to not use a star system. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
	FStarSystemDefinition StarSystem;

	/** Returns the sector with the given id, or nullptr when there is none. */
	const FSectorDefinition* FindSector(const FName SectorId) const;

	/** Returns true when a sector with the given id exists. */
	UFUNCTION(BlueprintPure, Category = "ProcSector")
	bool ContainsSector(const FName SectorId) const;

#if WITH_EDITOR
	/** Editor data validation: unique ids, presets assigned, every PlayAreaExtent component above 0, every gate leads to a real sector with a NormalizedLocation within -1 to 1 (warns when a gate is within 0.05 of the edge). */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
