#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "Data/SectorTypes.h"
#include "AsteroidField.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FHitResult;

/** Broadcast when an asteroid is removed. The parameter is the asteroid's stable id. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAsteroidRemoved, int32 /*AsteroidId*/);

/** Where one asteroid lives: which type's component holds it and at which instance index. */
struct FAsteroidSlot
{
	/** Index into FAsteroidFieldParams::Types, or INDEX_NONE for the untyped Instances component. */
	int32 TypeIndex = INDEX_NONE;

	/** Instance index inside that component. */
	int32 InstanceIndex = INDEX_NONE;
};

/**
 * Asteroids drawn as instanced meshes, so thousands cost a handful of draw calls.
 * With FAsteroidFieldParams::Types empty, every asteroid is an instance of the Instances component's mesh.
 * With types, each type gets its own instanced mesh component (created on demand and reused between sectors),
 * and each asteroid takes a type by weight.
 * They fill a flat belt, an ellipsoidal shell or the whole play-area box, chosen by FAsteroidFieldParams::Distribution;
 * every distance is a fraction of the play-area extent passed to BeginFill.
 * Filling is split into batches so it can be spread over several frames.
 * An asteroid's id is its generation order, which is stable for a given seed;
 * that is what makes "this asteroid was mined" survive a regeneration. Ids count across all types.
 */
UCLASS()
class PROCSECTOR_API AAsteroidField : public AActor
{
	GENERATED_BODY()

public:
	/** Creates the instanced mesh component and assigns the engine's basic cube mesh. */
	AAsteroidField();

	/**
	 * Clears the field and prepares a new seeded fill. Loads the type meshes and materials synchronously
	 * and sets up one component per type; unused components stay empty and hidden.
	 * PlayAreaExtent is the half-size of the sector's playable box; the fractions in Params are multiplied by it per axis.
	 * RemovedIds lists asteroids that must stay gone (taken from the sector state).
	 */
	void BeginFill(const FAsteroidFieldParams& Params, const FVector& PlayAreaExtent, const int32 Seed, const TArray<int32>& RemovedIds);

	/**
	 * Adds batches of instances until the field is complete or the deadline passes.
	 * DeadlineSeconds is an absolute FPlatformTime::Seconds value. Returns true when complete.
	 */
	bool FillSlice(const double DeadlineSeconds);

	/** Removes every instance from every component and cancels any fill in progress. */
	void ClearField();

	/** Hides one asteroid and broadcasts OnAsteroidRemoved. Returns false if the id is invalid or already removed. */
	bool RemoveAsteroid(const int32 AsteroidId);

	/** Returns the asteroid id a trace hit, on any type's component, or INDEX_NONE when the hit was not on this field. */
	int32 GetAsteroidIdFromHit(const FHitResult& Hit) const;

	/**
	 * Returns the asteroid's type index (-1 when Types is empty) and its world transform.
	 * False if the id is unknown or removed.
	 */
	bool GetAsteroidInfo(const int32 AsteroidId, int32& OutTypeIndex, FTransform& OutTransform) const;

	/** Number of asteroid slots generated so far, including removed ones. */
	int32 GetAsteroidCount() const;

	/** True while a fill has been started and is not yet complete. */
	bool IsFilling() const;

	/** Subscribe to learn about removed asteroids (the asteroid generation step records them). */
	FOnAsteroidRemoved OnAsteroidRemoved;

protected:
	/**
	 * Holds every asteroid instance when FAsteroidFieldParams::Types is empty; cleared and hidden otherwise.
	 * Assign a different mesh or material in a Blueprint subclass. Type components copy its collision and shadow settings.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	/** Vector parameter on the asteroid material receiving FAsteroidFieldParams::Tint, or each type's Tint. */
	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName TintParameter = TEXT("Color");

private:
	/**
	 * Builds the transform of the next asteroid from the random stream and reports its type.
	 * The main stream's draws are the same with or without types, so adding types never moves an asteroid or changes an id.
	 * With types, the type is drawn by weight from TypeStream and the three scale draws are mapped onto that type's range.
	 */
	FTransform MakeNextTransform(int32& OutTypeIndex);

	/** Draws the type of the next asteroid by weight from TypeStream. Always exactly one draw. */
	int32 PickTypeIndex();

	/** Creates type components until at least Count exist. Components are kept and reused between sectors. */
	void EnsureTypeComponents(const int32 Count);

	/** Gives the component for TypeIndex its mesh, material and tint, and shows it. */
	void SetUpTypeComponent(const int32 TypeIndex, const FAsteroidTypeParams& Type);

	/** The component holding asteroids of TypeIndex: Instances for INDEX_NONE, else that type's component. Null if missing. */
	UInstancedStaticMeshComponent* GetComponentForType(const int32 TypeIndex) const;

	/** Draws the next asteroid location for the Sphere distribution: an even-volume shell in unit space, scaled per axis into an ellipsoid. */
	FVector MakeSphereLocation();

	/** Draws the next asteroid location for the Box distribution: even over the box, redrawn while inside the central clearing. */
	FVector MakeBoxLocation();

	/** Most redraws the Box distribution makes for a point inside the central clearing before accepting the last draw. */
	static constexpr int32 MaxBoxRedraws = 16;

	/** Scale given to removed asteroids. Keeping the instance alive keeps every other id stable. */
	static constexpr float RemovedScale = 0.001f;

	/** Parameters of the fill in progress or last completed. */
	FAsteroidFieldParams ActiveParams;

	/** Play-area half-size of the fill in progress or last completed, in centimetres. */
	FVector ActivePlayAreaExtent = FVector::ZeroVector;

	/** Seeded random stream; advancing it in a fixed order is what makes the field repeatable. */
	FRandomStream Stream;

	/** Separate seeded stream for type picks, so choosing types never shifts the main stream's positions, rotations or scales. */
	FRandomStream TypeStream;

	/** Constant mixed into the fill seed for TypeStream. */
	static constexpr uint32 TypeSeedSalt = 0x7A9E5D13u;

	/** Ids that must be generated as removed during the current fill. */
	TArray<int32> PendingRemovedIds;

	/** Id the next generated asteroid will receive. Equals the number generated so far. */
	int32 NextAsteroidId = 0;

	/** True between BeginFill and the batch that completes the field. */
	bool bFilling = false;

	/** Runtime material instance of Instances that the tint is written to. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TintMID;

	/** One instanced mesh component per asteroid type, index-matched with FAsteroidFieldParams::Types. Extra ones are empty and hidden. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> TypeComponents;

	/** Runtime material instance of each type component, index-matched with TypeComponents. Null when the type has no material. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TypeMIDs;

	/** Sum of the active types' weights (negative weights count as 0). */
	float TotalTypeWeight = 0.0f;

	/** Id to slot: where each generated asteroid lives. Indexed by asteroid id. */
	TArray<FAsteroidSlot> Slots;

	/** Per type, instance index to asteroid id, for resolving hits. Index-matched with FAsteroidFieldParams::Types. */
	TArray<TArray<int32>> TypeInstanceIds;
};
