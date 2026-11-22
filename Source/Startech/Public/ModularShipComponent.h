// Ship Kit - builds a ship out of kit parts. Add it to your ship pawn (or use AModularShipActor),
// give it a Loadout, and call Rebuild() from the Construction Script so the ship shows in the editor.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "ShipLoadout.h"
#include "ModularShipComponent.generated.h"

class UStaticMeshComponent;

UCLASS(ClassGroup = (Ship), meta = (BlueprintSpawnableComponent))
class UModularShipComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UModularShipComponent();

	/** The parts to build. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TObjectPtr<UShipLoadout> Loadout = nullptr;

	/** Parts added on top of the loadout. A part with the same Id as a loadout part replaces it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TArray<FShipPartSpec> ExtraParts;

	/** Paint for material slot 0 of every part (overrides the loadout's). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	TObjectPtr<UMaterialInterface> Paint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName PartCollisionProfile = TEXT("BlockAllDynamic");

	/** Where the first-person camera goes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName PilotEyePart = TEXT("Hull");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ship")
	FName PilotEyeSocket = TEXT("PilotEye");

	/** Destroys the built parts and builds them again from Loadout + ExtraParts. */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void Rebuild();

	/** Puts Mesh on Parent.Socket under the name Id (replacing a part with that Id), then rebuilds. Works at runtime. */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void SetPart(FName Id, UStaticMesh* Mesh, FName Parent, FName Socket);

	/** Removes a part and everything plugged into it, then rebuilds. */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void RemovePart(FName Id);

	UFUNCTION(BlueprintPure, Category = "Ship")
	UStaticMeshComponent* GetPart(FName Id) const;

	/** Attaches e.g. your first-person camera to the pilot's eye (PilotEyePart.PilotEyeSocket). */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	bool AttachToPilotEye(USceneComponent* Component);

	/** World transforms of every "Muzzle" socket on the built parts (projectile spawn points), with the part ids. */
	UFUNCTION(BlueprintCallable, Category = "Ship")
	void GetMuzzles(TArray<FTransform>& Transforms, TArray<FName>& PartIds) const;

	/** The resolved part list (loadout + extras), in build order. */
	UFUNCTION(BlueprintPure, Category = "Ship")
	TArray<FShipPartSpec> GetPartSpecs() const;

protected:
	virtual void BeginPlay() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	void ClearBuilt();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UStaticMeshComponent>> Built;

	static const FString PartPrefix;
};

/** Ready-to-place ship: the component is the root and rebuilds whenever the actor is constructed. */
UCLASS(Blueprintable)
class AModularShipActor : public AActor
{
	GENERATED_BODY()

public:
	AModularShipActor();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ship")
	TObjectPtr<UModularShipComponent> Ship;
};
