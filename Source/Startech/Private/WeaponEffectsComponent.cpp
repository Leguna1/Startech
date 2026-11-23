#include "WeaponEffectsComponent.h"

#include "AmmoData.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "LaserMountComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ShieldDrainMountComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "StartechSettings.h"

namespace
{
	const FName ColorParameter(TEXT("User.Color"));
	const FName BeamEndParameter(TEXT("User.BeamEnd"));

	bool ExposesParameter(const UNiagaraComponent& Effect, const FNiagaraTypeDefinition& Type, const FName Name)
	{
		const UNiagaraSystem* const System = Effect.GetAsset();
		return System && System->GetExposedParameters().FindParameterOffset(FNiagaraVariable(Type, Name)) != nullptr;
	}

	void SetColor(UNiagaraComponent& Effect, const FLinearColor& Color)
	{
		if (ExposesParameter(Effect, FNiagaraTypeDefinition::GetColorDef(), ColorParameter))
		{
			Effect.SetVariableLinearColor(ColorParameter, Color);
		}
	}

	// Fire and forget: taken from the pool and returned to it when it finishes.
	void SpawnOneShotEffect(const UObject* const WorldContext, UNiagaraSystem* const System,
		const FVector& Location, const FRotator& Rotation, const FLinearColor& Color)
	{
		if (!System)
		{
			return;
		}

		UNiagaraComponent* const Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System,
			Location, Rotation, FVector(1.f), true, false, ENCPoolMethod::AutoRelease);
		if (Effect)
		{
			SetColor(*Effect, Color);
			Effect->Activate(true);
		}
	}

	void PlayOneShotSound(const UObject* const WorldContext, USoundBase* const Sound, const FVector& Location)
	{
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location);
		}
	}

	// Back to the pool, or destroyed if pooling is turned off and it was created fresh.
	void ReturnBolt(UNiagaraComponent& Bolt)
	{
		if (Bolt.PoolingMethod == ENCPoolMethod::ManualRelease)
		{
			Bolt.ReleaseToPool();
		}
		else
		{
			Bolt.DestroyComponent();
		}
	}

	// For when this component won't be around to hear OnSystemFinished: the engine deactivates the bolt
	// and releases (or destroys) it itself when it completes.
	void HandOffBolt(UNiagaraComponent& Bolt)
	{
		if (!Bolt.IsActive() || Bolt.PoolingMethod == ENCPoolMethod::ManualRelease)
		{
			ReturnBolt(Bolt);
			return;
		}

		Bolt.SetAutoDestroy(true);
		Bolt.Deactivate();
	}

	// Reuses Existing when it already plays System, otherwise replaces it. Null when System is.
	// The result is attached to Parent and not yet active.
	UNiagaraComponent* PrepareAttachedEffect(UNiagaraComponent* const Existing, UNiagaraSystem* const System,
		USceneComponent* const Parent)
	{
		if (IsValid(Existing) && Existing->GetAsset() == System)
		{
			return Existing;
		}

		if (IsValid(Existing))
		{
			Existing->DestroyComponent();
		}

		if (!System)
		{
			return nullptr;
		}

		return UNiagaraFunctionLibrary::SpawnSystemAttached(System, Parent, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false, false);
	}

	// The beam and hit effects are kept for reuse; the sound is stopped and let go.
	void StopBeam(FDrainBeamEffect& Effect)
	{
		if (IsValid(Effect.Beam))
		{
			Effect.Beam->Deactivate();
		}

		if (IsValid(Effect.Hit))
		{
			Effect.Hit->Deactivate();
		}

		if (IsValid(Effect.Sound))
		{
			Effect.Sound->Stop();
		}
		Effect.Sound = nullptr;
	}
}

UWeaponEffectsComponent::UWeaponEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponEffectsComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<ULaserMountComponent*> LaserMounts;
	Owner->GetComponents(LaserMounts);
	for (ULaserMountComponent* const Mount : LaserMounts)
	{
		Mount->OnShotFired.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleShotFired);
		Mount->OnShotMoved.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleShotMoved);
		Mount->OnShotEnded.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleShotEnded);
	}

	TArray<UShieldDrainMountComponent*> DrainMounts;
	Owner->GetComponents(DrainMounts);
	for (UShieldDrainMountComponent* const Mount : DrainMounts)
	{
		Mount->OnBeamStarted.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleBeamStarted);
		Mount->OnBeamUpdated.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleBeamUpdated);
		Mount->OnBeamStopped.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleBeamStopped);
	}
}

void UWeaponEffectsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const FLaserBoltEffect& Effect : Bolts)
	{
		if (IsValid(Effect.Sound))
		{
			Effect.Sound->Stop();
		}

		if (IsValid(Effect.Bolt))
		{
			HandOffBolt(*Effect.Bolt);
		}
	}
	Bolts.Reset();

	for (UNiagaraComponent* const Bolt : FinishingBolts)
	{
		if (IsValid(Bolt))
		{
			Bolt->OnSystemFinished.RemoveDynamic(this, &UWeaponEffectsComponent::HandleBoltFinished);
			HandOffBolt(*Bolt);
		}
	}
	FinishingBolts.Reset();

	for (FDrainBeamEffect& Effect : Beams)
	{
		StopBeam(Effect);
	}

	Super::EndPlay(EndPlayReason);
}

void UWeaponEffectsComponent::HandleShotFired(ULaserMountComponent* const Mount, const int32 ShotId, const FVector Start,
	UAmmoData* const Ammo)
{
	// Without ammo a shot has no look to show.
	if (!Ammo)
	{
		return;
	}

	FRotator Rotation = FRotator::ZeroRotator;
	if (Mount)
	{
		Mount->GetShotRotation(ShotId, Rotation);
	}

	SpawnOneShotEffect(this, Ammo->StartEffect, Start, Rotation, Ammo->Color);
	PlayStartSound(Ammo->StartSound, Start);

	FLaserBoltEffect Effect;
	Effect.ShotId = ShotId;
	Effect.Ammo = Ammo;
	Effect.Rotation = Rotation;

	if (Ammo->TravelEffect)
	{
		// Pooled, but released by hand when the shot ends.
		Effect.Bolt = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Ammo->TravelEffect,
			Start, Rotation, FVector(1.f), false, false, ENCPoolMethod::ManualRelease);
		if (Effect.Bolt)
		{
			SetColor(*Effect.Bolt, Ammo->Color);
			Effect.Bolt->Activate(true);
		}
	}

	if (Ammo->TravelSound)
	{
		// With no bolt effect to ride on, the sound is moved along the path by itself.
		Effect.Sound = Effect.Bolt
			? UGameplayStatics::SpawnSoundAttached(Ammo->TravelSound, Effect.Bolt)
			: UGameplayStatics::SpawnSoundAtLocation(this, Ammo->TravelSound, Start, Rotation);
	}

	Bolts.Add(Effect);
}

void UWeaponEffectsComponent::PlayStartSound(USoundBase* const Sound, const FVector& Location)
{
	const UWorld* const World = GetWorld();
	if (!Sound || !World)
	{
		return;
	}

	const FStartechAudioSettings& Settings = UStartechSettings::Get().Audio;
	const double Now = World->GetTimeSeconds();
	if (Sound == LastStartSound && LastStartSoundTime >= 0.0 && Now - LastStartSoundTime <= Settings.VolleyWindowSeconds)
	{
		return;
	}
	LastStartSound = Sound;
	LastStartSoundTime = Now;
	++StartSoundsPlayed;

	// From a chase camera the player's own guns would sound distant if placed at the mounts.
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled())
	{
		UGameplayStatics::PlaySound2D(this, Sound, Settings.PlayerLaserStartVolume);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
}

void UWeaponEffectsComponent::HandleShotMoved(ULaserMountComponent* const Mount, const int32 ShotId, const FVector Location, const FRotator Rotation)
{
	const int32 Index = FindBolt(ShotId);
	if (Index == INDEX_NONE)
	{
		return;
	}

	FLaserBoltEffect& Effect = Bolts[Index];
	Effect.Rotation = Rotation;

	if (IsValid(Effect.Bolt))
	{
		Effect.Bolt->SetWorldLocationAndRotation(Location, Rotation);
	}
	else if (IsValid(Effect.Sound))
	{
		Effect.Sound->SetWorldLocation(Location);
	}
}

void UWeaponEffectsComponent::HandleShotEnded(ULaserMountComponent* const Mount, const int32 ShotId, const FVector Location, const bool bImpact)
{
	const int32 Index = FindBolt(ShotId);
	if (Index == INDEX_NONE)
	{
		return;
	}

	const FLaserBoltEffect Effect = Bolts[Index];
	Bolts.RemoveAtSwap(Index);
	EndBolt(Effect);

	if (bImpact && Effect.Ammo)
	{
		SpawnOneShotEffect(this, Effect.Ammo->HitEffect, Location, Effect.Rotation, Effect.Ammo->Color);
		PlayOneShotSound(this, Effect.Ammo->HitSound, Location);
	}
}

void UWeaponEffectsComponent::EndBolt(const FLaserBoltEffect& Effect)
{
	if (IsValid(Effect.Sound))
	{
		Effect.Sound->Stop();
	}

	UNiagaraComponent* const Bolt = Effect.Bolt;
	if (!IsValid(Bolt))
	{
		return;
	}

	// Already finished on its own: nothing left to wait for.
	if (!Bolt->IsActive())
	{
		ReturnBolt(*Bolt);
		return;
	}

	// Bound first, because Deactivate can finish the system immediately when nothing is left alive.
	FinishingBolts.Add(Bolt);
	Bolt->OnSystemFinished.AddUniqueDynamic(this, &UWeaponEffectsComponent::HandleBoltFinished);

	// Each emitter follows its own Inactive Response from the asset: Kill emitters vanish now,
	// Complete emitters (such as a trail) let their particles live out their lifetime.
	Bolt->Deactivate();
}

void UWeaponEffectsComponent::HandleBoltFinished(UNiagaraComponent* const Bolt)
{
	if (!Bolt || FinishingBolts.RemoveSwap(Bolt) == 0)
	{
		return;
	}

	// The pool hands this component out again, so it must not keep calling back here.
	Bolt->OnSystemFinished.RemoveDynamic(this, &UWeaponEffectsComponent::HandleBoltFinished);
	ReturnBolt(*Bolt);
}

void UWeaponEffectsComponent::HandleBeamStarted(UShieldDrainMountComponent* const Mount, UAmmoData* const Ammo)
{
	if (!Mount)
	{
		return;
	}

	int32 Index = FindBeam(Mount);
	if (Index == INDEX_NONE)
	{
		Index = Beams.AddDefaulted();
		Beams[Index].Mount = Mount;
	}

	FDrainBeamEffect& Effect = Beams[Index];
	Effect.Ammo = Ammo;

	// Without ammo the beam has no look to show.
	if (!Ammo)
	{
		return;
	}

	const FVector Start = Mount->GetComponentLocation();
	SpawnOneShotEffect(this, Ammo->StartEffect, Start, Mount->GetComponentRotation(), Ammo->Color);
	PlayOneShotSound(this, Ammo->StartSound, Start);

	Effect.Beam = PrepareAttachedEffect(Effect.Beam, Ammo->TravelEffect, Mount);
	if (Effect.Beam)
	{
		SetColor(*Effect.Beam, Ammo->Color);
		Effect.Beam->Activate(true);
	}

	// Activated by HandleBeamUpdated once the beam hits something.
	Effect.Hit = PrepareAttachedEffect(Effect.Hit, Ammo->HitEffect, Mount);
	if (Effect.Hit)
	{
		SetColor(*Effect.Hit, Ammo->Color);
	}

	if (Ammo->TravelSound)
	{
		if (IsValid(Effect.Sound))
		{
			Effect.Sound->Stop();
		}
		Effect.Sound = UGameplayStatics::SpawnSoundAttached(Ammo->TravelSound, Mount);
	}
}

void UWeaponEffectsComponent::HandleBeamUpdated(UShieldDrainMountComponent* const Mount, const FVector Start, const FVector End,
	const bool bHit)
{
	const int32 Index = FindBeam(Mount);
	if (Index == INDEX_NONE)
	{
		return;
	}

	const FDrainBeamEffect& Effect = Beams[Index];

	if (UNiagaraComponent* const Beam = Effect.Beam; IsValid(Beam) && Beam->IsActive())
	{
		// Accepts the parameter as a plain vector or as a Niagara position, which is the same thing in world space.
		if (ExposesParameter(*Beam, FNiagaraTypeDefinition::GetVec3Def(), BeamEndParameter))
		{
			Beam->SetVariableVec3(BeamEndParameter, End);
		}
		else if (ExposesParameter(*Beam, FNiagaraTypeDefinition::GetPositionDef(), BeamEndParameter))
		{
			Beam->SetVariablePosition(BeamEndParameter, End);
		}
	}

	if (UNiagaraComponent* const Hit = Effect.Hit; IsValid(Hit))
	{
		if (bHit)
		{
			// Faces back along the beam, toward the mount.
			Hit->SetWorldLocationAndRotation(End, (Start - End).Rotation());
			if (!Hit->IsActive())
			{
				Hit->Activate(true);
			}
		}
		else if (Hit->IsActive())
		{
			Hit->Deactivate();
		}
	}
}

void UWeaponEffectsComponent::HandleBeamStopped(UShieldDrainMountComponent* const Mount)
{
	if (const int32 Index = FindBeam(Mount); Index != INDEX_NONE)
	{
		StopBeam(Beams[Index]);
	}
}

int32 UWeaponEffectsComponent::FindBolt(const int32 ShotId) const
{
	return Bolts.IndexOfByPredicate([ShotId](const FLaserBoltEffect& Each) { return Each.ShotId == ShotId; });
}

int32 UWeaponEffectsComponent::FindBeam(const UShieldDrainMountComponent* const Mount) const
{
	return Beams.IndexOfByPredicate([Mount](const FDrainBeamEffect& Each) { return Each.Mount == Mount; });
}
