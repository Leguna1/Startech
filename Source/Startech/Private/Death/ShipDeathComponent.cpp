#include "Death/ShipDeathComponent.h"

#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechDeath, Log, All);

namespace ShipDeath
{
	/** The tumble starts at this share of the maximum and builds up linearly. */
	constexpr float TumbleStartShare = 0.1f;

	/** Secondary explosions go off between these shares of the dying state. */
	constexpr float SecondaryWindowStart = 0.15f;
	constexpr float SecondaryWindowEnd = 0.85f;

	/** Each secondary explosion adds a flash pulse this strong, fading over PulseSeconds. */
	constexpr float PulseStrength = 0.35f;
	constexpr float PulseSeconds = 0.12f;

	/** Random hull points stay inside this share of the mesh's bounds. */
	constexpr float HullPointShare = 0.7f;

	UNiagaraSystem* LoadSystem(const TSoftObjectPtr<UNiagaraSystem>& System)
	{
		return System.IsNull() ? nullptr : System.LoadSynchronous();
	}
}

FShipDeathEffects FShipDeathEffects::Resolve(const FShipDeathEffects& Override, const FShipDeathEffects& Default)
{
	FShipDeathEffects Out;
	Out.DeathExplosionFX = Override.DeathExplosionFX.IsNull() ? Default.DeathExplosionFX : Override.DeathExplosionFX;
	Out.SecondaryExplosionFX = Override.SecondaryExplosionFX.IsNull() ? Default.SecondaryExplosionFX : Override.SecondaryExplosionFX;
	Out.DeathSound = Override.DeathSound.IsNull() ? Default.DeathSound : Override.DeathSound;
	Out.DeathExplosionScale = FMath::Max(Override.DeathExplosionScale > 0.f ? Override.DeathExplosionScale
		: (Default.DeathExplosionScale > 0.f ? Default.DeathExplosionScale : 1.f), 0.01f);
	Out.SecondaryExplosionScale = FMath::Max(Override.SecondaryExplosionScale > 0.f ? Override.SecondaryExplosionScale
		: (Default.SecondaryExplosionScale > 0.f ? Default.SecondaryExplosionScale : 1.f), 0.01f);
	return Out;
}

UShipDeathComponent::UShipDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// After the owner's tick, so the tumble is applied over the flight model's move.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UShipDeathComponent::BeginDying(const FShipDeathEffects& TypeEffects)
{
	if (State != EState::Alive || !GetOwner())
	{
		return;
	}

	const FShipDeathSettings& Settings = UStartechSettings::Get().Death;
	Effects = FShipDeathEffects::Resolve(TypeEffects, Settings.DefaultEffects);
	Duration = FMath::Max(Settings.DeathDuration, 0.f);
	Elapsed = 0.f;
	Flash = 0.f;
	SincePulse = 1000.f;
	TumbleAxis = FMath::VRand();
	State = EState::Dying;

	MakeWreckCollision();

	UE_LOG(LogStartechDeath, Log, TEXT("[Death] %s dying for %.2f s at %s"), *GetOwner()->GetName(), Duration,
		*GetOwner()->GetActorLocation().ToCompactString());

	if (Duration <= 0.f)
	{
		Explode();
		return;
	}

	StartFlash();
	ScheduleSecondaries();
	SetComponentTickEnabled(true);
}

void UShipDeathComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* const ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (State != EState::Dying)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Elapsed += DeltaTime;
	SincePulse += DeltaTime;

	if (AActor* const Owner = GetOwner())
	{
		const float Step = FMath::DegreesToRadians(GetTumbleRate() * DeltaTime);
		Owner->AddActorWorldRotation(FQuat(TumbleAxis, Step));
	}

	while (SecondaryTimes.Num() > 0 && Elapsed >= SecondaryTimes[0])
	{
		SecondaryTimes.RemoveAt(0);
		FireSecondary();
	}

	if (Elapsed >= Duration)
	{
		Explode();
		return;
	}

	ApplyFlash();
}

float UShipDeathComponent::GetTumbleRate() const
{
	if (State == EState::Alive || Duration <= 0.f)
	{
		return 0.f;
	}

	const float Progress = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	return UStartechSettings::Get().Death.MaxTumbleRate * FMath::Lerp(ShipDeath::TumbleStartShare, 1.f, Progress);
}

void UShipDeathComponent::ResetDeath()
{
	SetComponentTickEnabled(false);
	RestoreCollision();
	RestoreMaterials();

	State = EState::Alive;
	Elapsed = 0.f;
	Duration = 0.f;
	Flash = 0.f;
	SincePulse = 1000.f;
	SecondaryTimes.Reset();
}

void UShipDeathComponent::OnAcquiredFromPool_Implementation()
{
	ResetDeath();
}

void UShipDeathComponent::OnReleasedToPool_Implementation()
{
	ResetDeath();
}

void UShipDeathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SecondaryTimes.Reset();
	Super::EndPlay(EndPlayReason);
}

void UShipDeathComponent::MakeWreckCollision()
{
	RestoreCollision();

	TInlineComponentArray<UPrimitiveComponent*> Primitives(GetOwner());
	for (UPrimitiveComponent* const Primitive : Primitives)
	{
		if (!Primitive || Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		FSavedCollision& Saved = SavedCollision.AddDefaulted_GetRef();
		Saved.Primitive = Primitive;
		Saved.Responses = Primitive->GetCollisionResponseToChannels();
		Saved.bGenerateOverlapEvents = Primitive->GetGenerateOverlapEvents();

		// Shots trace Visibility; ships are Pawns. The world still blocks, so the wreck doesn't drift through rocks.
		Primitive->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Primitive->SetGenerateOverlapEvents(false);
	}
}

void UShipDeathComponent::RestoreCollision()
{
	for (const FSavedCollision& Saved : SavedCollision)
	{
		if (UPrimitiveComponent* const Primitive = Saved.Primitive.Get())
		{
			Primitive->SetCollisionResponseToChannels(Saved.Responses);
			Primitive->SetGenerateOverlapEvents(Saved.bGenerateOverlapEvents);
		}
	}
	SavedCollision.Reset();
}

UMeshComponent* UShipDeathComponent::GetMesh() const
{
	const AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// The root is the ship's hull (AStartechShipBase::ShipMesh); any other ship type gets its first mesh.
	if (UMeshComponent* const Root = Cast<UMeshComponent>(Owner->GetRootComponent()))
	{
		return Root;
	}
	return Owner->FindComponentByClass<UMeshComponent>();
}

void UShipDeathComponent::StartFlash()
{
	RestoreMaterials();

	UMeshComponent* const Mesh = GetMesh();
	if (!Mesh)
	{
		return;
	}

	const FShipDeathSettings& Settings = UStartechSettings::Get().Death;
	const int32 NumSlots = Mesh->GetNumMaterials();
	if (NumSlots <= 0)
	{
		return;
	}

	FlashMesh = Mesh;
	SavedOverrides = Mesh->OverrideMaterials;
	bMaterialsChanged = true;

	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		UMaterialInstanceDynamic* const Material = Mesh->CreateDynamicMaterialInstance(Slot);
		if (!Material)
		{
			continue;
		}
		FlashMaterials.Add(Material);

		FFlashSlot& Flashing = FlashSlots.AddDefaulted_GetRef();
		Flashing.Material = Material;
		for (const FName Parameter : Settings.FlashColorParameters)
		{
			FLinearColor Value;
			if (!Parameter.IsNone() && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Value))
			{
				Flashing.Colors.Emplace(Parameter, Value);
			}
		}
		if (!Settings.FlashEmissionParameter.IsNone())
		{
			Flashing.bHasEmission = Material->GetScalarParameterValue(
				FHashedMaterialParameterInfo(Settings.FlashEmissionParameter), Flashing.Emission);
		}
	}
}

void UShipDeathComponent::ApplyFlash()
{
	if (FlashSlots.Num() == 0 || Duration <= 0.f)
	{
		return;
	}

	const FShipDeathSettings& Settings = UStartechSettings::Get().Death;
	const float Progress = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);

	// Builds up over the second, flickering, with a short pulse at each secondary explosion.
	const float Flicker = 0.5f + 0.5f * FMath::Sin(2.f * PI * Settings.FlashFlickerHz * Elapsed);
	const float Pulse = ShipDeath::PulseStrength * FMath::Clamp(1.f - SincePulse / ShipDeath::PulseSeconds, 0.f, 1.f);
	Flash = FMath::Clamp(Progress * Progress * (0.6f + 0.4f * Flicker) + Pulse, 0.f, 1.f);

	const float White = Settings.FlashMaxWhite * Flash;
	const float EmissionScale = 1.f + Settings.FlashEmissionBoost * Flash;

	for (const FFlashSlot& Flashing : FlashSlots)
	{
		UMaterialInstanceDynamic* const Material = Flashing.Material.Get();
		if (!Material)
		{
			continue;
		}
		for (const TPair<FName, FLinearColor>& Color : Flashing.Colors)
		{
			Material->SetVectorParameterValue(Color.Key, FMath::Lerp(Color.Value, FLinearColor::White, White));
		}
		if (Flashing.bHasEmission)
		{
			// A base of 0 would never light up, so the boost starts from at least 1.
			Material->SetScalarParameterValue(Settings.FlashEmissionParameter,
				FMath::Max(Flashing.Emission, 1.f) * EmissionScale);
		}
	}
}

void UShipDeathComponent::RestoreMaterials()
{
	if (bMaterialsChanged)
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

	bMaterialsChanged = false;
	FlashMesh.Reset();
	SavedOverrides.Reset();
	FlashMaterials.Reset();
	FlashSlots.Reset();
}

void UShipDeathComponent::ScheduleSecondaries()
{
	SecondaryTimes.Reset();

	const FShipDeathSettings& Settings = UStartechSettings::Get().Death;
	const int32 Min = FMath::Max(Settings.MinSecondaryExplosions, 0);
	const int32 Count = FMath::RandRange(Min, FMath::Max(Settings.MaxSecondaryExplosions, Min));
	if (Count <= 0)
	{
		return;
	}

	// One in each equal part of the window, so they never bunch up.
	const float Start = Duration * ShipDeath::SecondaryWindowStart;
	const float Part = Duration * (ShipDeath::SecondaryWindowEnd - ShipDeath::SecondaryWindowStart) / Count;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		SecondaryTimes.Add(Start + Part * (Index + FMath::FRand()));
	}
}

void UShipDeathComponent::FireSecondary()
{
	const FVector Location = GetRandomHullPoint();
	SincePulse = 0.f;

	if (UNiagaraSystem* const System = ShipDeath::LoadSystem(Effects.SecondaryExplosionFX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, FRotator::ZeroRotator,
			FVector(Effects.SecondaryExplosionScale));
	}

	OnSecondaryExplosion.Broadcast(Location);
}

void UShipDeathComponent::Explode()
{
	AActor* const Owner = GetOwner();
	if (State != EState::Dying || !Owner)
	{
		return;
	}

	State = EState::Exploded;
	SetComponentTickEnabled(false);
	SecondaryTimes.Reset();

	const FVector Location = Owner->GetActorLocation();

	if (UNiagaraSystem* const System = ShipDeath::LoadSystem(Effects.DeathExplosionFX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, FRotator::ZeroRotator,
			FVector(Effects.DeathExplosionScale));
	}
	if (USoundBase* const Sound = Effects.DeathSound.IsNull() ? nullptr : Effects.DeathSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}

	UE_LOG(LogStartechDeath, Log, TEXT("[Death] %s exploded after %.2f s at %s"), *Owner->GetName(), Elapsed,
		*Location.ToCompactString());

	// Hidden with the flash still on; ResetDeath puts the materials back before the ship is seen again.
	Owner->SetActorHiddenInGame(true);

	OnExploded.Broadcast(Location);
}

FVector UShipDeathComponent::GetRandomHullPoint() const
{
	const AActor* const Owner = GetOwner();
	const UMeshComponent* const Mesh = GetMesh();
	if (!Mesh)
	{
		return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	}

	const FBox Local = Mesh->CalcLocalBounds().GetBox();
	const FVector Center = Local.GetCenter();
	const FVector Extent = Local.GetExtent() * ShipDeath::HullPointShare;
	const FVector Point = Center + FVector(FMath::FRandRange(-1.f, 1.f) * Extent.X,
		FMath::FRandRange(-1.f, 1.f) * Extent.Y, FMath::FRandRange(-1.f, 1.f) * Extent.Z);
	return Mesh->GetComponentTransform().TransformPosition(Point);
}
