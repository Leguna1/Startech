#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Death/HullFlash.h"
#include "Death/ShipDeathTypes.h"
#include "Engine/EngineTypes.h"
#include "Population/Poolable.h"
#include "ShipDeathComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;

/** A location in the world: where a secondary explosion went off, or where the ship exploded. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShipDeathLocation, const FVector& /* Location */);

/**
 * The dying state between hull 0 and removal, for every ship (subobject ShipDeath on AStartechShipBase).
 *
 * BeginDying starts it once the ship has done everything that counts as the death (credited, stopped, untargetable).
 * For UStartechSettings::Death.DeathDuration the wreck keeps its velocity (the ship's flight model only moves it), its
 * tumble builds up, its hull flashes towards white and secondary explosions go off on it. Its collision ignores pawns
 * and shots and makes no overlaps, so it blocks nothing and can't dock, warp or collect. Then the final explosion is
 * spawned, the actor is hidden and OnExploded fires, where the ship drops its loot and leaves play.
 *
 * Driven by the component's own tick, so it pauses with the game. ResetDeath puts back everything it changed
 * (collision, materials, tick); it runs from the pool hooks and the player's respawn.
 */
UCLASS(ClassGroup = (Startech))
class STARTECH_API UShipDeathComponent : public UActorComponent, public IPoolable
{
	GENERATED_BODY()

public:
	UShipDeathComponent();

	/** Starts the dying state with TypeEffects over the settings' defaults. Ignored unless alive. */
	void BeginDying(const FShipDeathEffects& TypeEffects);

	/** Back to alive: collision, materials and tick as before BeginDying, nothing scheduled. Idempotent. */
	void ResetDeath();

	/** Neither dying nor exploded. */
	bool IsAlive() const { return State == EState::Alive; }

	/** Between BeginDying and the final explosion. */
	bool IsDying() const { return State == EState::Dying; }

	/** The final explosion has gone off; the ship is hidden and waits to leave play or respawn. */
	bool HasExploded() const { return State == EState::Exploded; }

	/** Seconds since BeginDying (0 when alive). */
	float GetElapsed() const { return Elapsed; }

	/** The current tumble in degrees per second (0 unless dying). */
	float GetTumbleRate() const;

	/** The current flash, 0 to 1 (0 unless dying). */
	float GetFlash() const { return Flash; }

	/** The effects this death uses (resolved in BeginDying). */
	const FShipDeathEffects& GetEffects() const { return Effects; }

	/** Each secondary explosion, after its effect is spawned. */
	FOnShipDeathLocation OnSecondaryExplosion;

	/** The end of the dying state: the final explosion was spawned and the actor hidden. The ship drops its loot here. */
	FOnShipDeathLocation OnExploded;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReleasedToPool_Implementation() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class EState : uint8
	{
		Alive,
		Dying,
		Exploded
	};

	/** One primitive's collision before the death. */
	struct FSavedCollision
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		FCollisionResponseContainer Responses;
		bool bGenerateOverlapEvents = false;
	};

	/** Pawns and shots pass through every colliding primitive of the owner, and nothing overlaps it. */
	void MakeWreckCollision();
	void RestoreCollision();

	/** Dynamic materials on the owner's mesh, remembering the overrides they replace. */
	void StartFlash();
	void ApplyFlash();
	void RestoreMaterials();

	/** Spreads the secondary explosions over the middle of the dying state. */
	void ScheduleSecondaries();
	void FireSecondary();

	/** The final explosion, then hides the owner and broadcasts OnExploded. */
	void Explode();

	/** A random point inside the owner mesh's bounds, in world space; the actor's location without a mesh. */
	FVector GetRandomHullPoint() const;

	UMeshComponent* GetMesh() const;

	EState State = EState::Alive;
	float Elapsed = 0.f;
	float Duration = 0.f;
	float Flash = 0.f;

	/** Seconds since the last secondary explosion, for the flash's pulse. */
	float SincePulse = 1000.f;

	FVector TumbleAxis = FVector::UpVector;

	/** Times (seconds since BeginDying) of the secondary explosions still to go off, earliest first. */
	TArray<float> SecondaryTimes;

	UPROPERTY(Transient)
	FShipDeathEffects Effects;

	TArray<FSavedCollision> SavedCollision;

	/** The white flash on the hull's materials. */
	UPROPERTY(Transient)
	FHullFlash HullFlash;
};
