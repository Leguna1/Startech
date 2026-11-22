#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Population/Poolable.h"
#include "Station.generated.h"

class AStartechShipPawn;
class UMapMarkerComponent;
class UPrimitiveComponent;
class USceneComponent;
class UShopData;
class USphereComponent;
class UStaticMeshComponent;
struct FHitResult;

// A safe harbour: the player docks by flying into its zone, is repaired and protected there,
// can trade at its shops, and respawns at the nearest one after death.
// Pooled when a sector generates it. It cannot be destroyed, so it has no per-life state beyond the docked ship.
UCLASS()
class STARTECH_API AStation : public AActor, public IPoolable
{
	GENERATED_BODY()

public:
	AStation();

	virtual void Tick(float DeltaSeconds) override;

	const TArray<TObjectPtr<UShopData>>& GetShops() const { return Shops; }
	AStartechShipPawn* GetDockedShip() const { return DockedShip.Get(); }

	/** Pool hook: docks a player ship that is already inside the zone where the station was placed. */
	virtual void OnAcquiredFromPool_Implementation() override;

	/** Pool hook: undocks the player, so a stored station never leaves them invulnerable. */
	virtual void OnReleasedToPool_Implementation() override;

	/** The station's name for the HUD: its map marker's label, or "Station" when that is empty. */
	FText GetDisplayName() const;

	// Where a destroyed player reappears.
	FTransform GetRespawnTransform() const;

	/**
	 * Where Ship goes when it undocks: just outside the docking zone (its radius plus the ship's collision radius plus
	 * UndockClearance), on the line from the zone's centre through the ship, at the ship's height, facing away from
	 * the station. Straight ahead of the station when the ship is at its centre.
	 */
	FTransform GetUndockTransform(const AActor& Ship) const;

	/**
	 * The station nearest From that is in play, or null if the sector has none. Stations stored in the pool are
	 * still in the world, hidden, and are skipped.
	 */
	static AStation* FindNearestInPlay(const UWorld& World, const FVector& From);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<USceneComponent> Root;

	// Assign the mesh in the Blueprint.
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Overlaps pawns only. The player is docked while inside it.
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<USphereComponent> DockingZone;

	// Place it in the Blueprint, inside the docking zone, so a respawned player starts docked.
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<USceneComponent> RespawnPoint;

	// On the radar rim when out of range, and on screen; only the nearest station's screen marker shows.
	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<UMapMarkerComponent> MapMarker;

	// One tab each on the shop screen.
	UPROPERTY(EditAnywhere, Category = "Station")
	TArray<TObjectPtr<UShopData>> Shops;

	// Gap left between the docking zone's edge and an undocked ship's collision, in cm.
	UPROPERTY(EditAnywhere, Category = "Station", meta = (ClampMin = "0"))
	float UndockClearance = 500.f;

	// Hull per second while docked.
	UPROPERTY(EditAnywhere, Category = "Station", meta = (ClampMin = "0"))
	float RepairRate = 25.f;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void Dock(AStartechShipPawn& Ship);

	/** Docks the first player ship found inside the docking zone, if any. Overlap events miss ships already inside. */
	void DockOverlappingShip();
	void Undock();

	TWeakObjectPtr<AStartechShipPawn> DockedShip;
};
