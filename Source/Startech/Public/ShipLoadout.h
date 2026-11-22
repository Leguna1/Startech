// Ship Kit - a loadout: which part goes on which socket of which other part.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShipLoadout.generated.h"

class UStaticMesh;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FShipPartSpec
{
	GENERATED_BODY()

	/** Unique name of this part in the ship, e.g. "Hull", "WingL", "GunL". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Id of the part this one plugs into. None = the root part (the hull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName Parent;

	/** Socket on the parent part's mesh, e.g. "Wing_L", "Tip", "Hardpoint". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName Socket;
};

UCLASS(BlueprintType)
class UShipLoadout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TArray<FShipPartSpec> Parts;

	/** Optional paint: replaces material slot 0 (the palette) on every part, e.g. MI_Ship_Red. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UMaterialInterface> Paint = nullptr;
};
