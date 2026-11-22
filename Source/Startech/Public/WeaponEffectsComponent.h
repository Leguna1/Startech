#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponEffectsComponent.generated.h"

class UAmmoData;
class UAudioComponent;
class ULaserMountComponent;
class UNiagaraComponent;
class UShieldDrainMountComponent;

// The effect and sound of one laser bolt in flight.
USTRUCT()
struct FLaserBoltEffect
{
	GENERATED_BODY()

	int32 ShotId = 0;

	// The ammo the shot was fired with, for its hit effect and sound.
	UPROPERTY()
	TObjectPtr<const UAmmoData> Ammo;

	// The bolt's last direction of travel, used to orient its hit.
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Bolt;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Sound;
};

// The beam, hit effect and loop sound of one shield-drain mount.
USTRUCT()
struct FDrainBeamEffect
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UShieldDrainMountComponent> Mount;

	// The ammo the running beam started with.
	UPROPERTY()
	TObjectPtr<const UAmmoData> Ammo;

	// Beam and Hit are kept between uses and reactivated while the ammo's effects stay the same.
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Beam;

	// Held at the beam's end point while the beam hits something.
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Hit;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Sound;
};

// Spawns every effect and sound for its owner's laser and shield-drain mounts. Colours, effects and sounds
// all come from the ammo each event carries; weapons define mechanics only.
// The mounts only report what happens; nothing visual lives in them.
UCLASS(ClassGroup = (Startech), meta = (BlueprintSpawnableComponent))
class STARTECH_API UWeaponEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEffectsComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleShotFired(ULaserMountComponent* Mount, int32 ShotId, FVector Start, UAmmoData* Ammo);

	UFUNCTION()
	void HandleShotMoved(ULaserMountComponent* Mount, int32 ShotId, FVector Location, FRotator Rotation);

	UFUNCTION()
	void HandleShotEnded(ULaserMountComponent* Mount, int32 ShotId, FVector Location, bool bImpact);

	// Bound to a bolt effect's OnSystemFinished after its shot ends.
	UFUNCTION()
	void HandleBoltFinished(UNiagaraComponent* Bolt);

	UFUNCTION()
	void HandleBeamStarted(UShieldDrainMountComponent* Mount, UAmmoData* Ammo);

	UFUNCTION()
	void HandleBeamUpdated(UShieldDrainMountComponent* Mount, FVector Start, FVector End, bool bHit);

	UFUNCTION()
	void HandleBeamStopped(UShieldDrainMountComponent* Mount);

	// Stops the bolt's sound and deactivates its effect, which returns to the pool once it reports finished.
	void EndBolt(const FLaserBoltEffect& Effect);

	// Index into Bolts, or INDEX_NONE.
	int32 FindBolt(int32 ShotId) const;

	// Index into Beams, or INDEX_NONE.
	int32 FindBeam(const UShieldDrainMountComponent* Mount) const;

	UPROPERTY(Transient)
	TArray<FLaserBoltEffect> Bolts;

	// Bolt effects whose shot has ended, deactivated and waiting for OnSystemFinished.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> FinishingBolts;

	// One entry per shield-drain mount that has fired.
	UPROPERTY(Transient)
	TArray<FDrainBeamEffect> Beams;
};
