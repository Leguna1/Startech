#include "StartechEnemyShip.h"

#include "Combat/CombatBalance.h"
#include "Combat/EnemyChargeComponent.h"
#include "Death/ShipDeathComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DespawnSubsystem.h"
#include "EnemyBrainComponent.h"
#include "EnemyData.h"
#include "HealthComponent.h"
#include "LaserMountComponent.h"
#include "LaserWeaponData.h"
#include "ShieldDrainMountComponent.h"
#include "LootPickup.h"
#include "LootTable.h"
#include "MapMarkerComponent.h"
#include "StartechQuestBridge.h"
#include "TargetableComponent.h"
#include "Tiers/StartechTierSubsystem.h"
#include "WeaponSystemComponent.h"

FOnAnyEnemyDied AStartechEnemyShip::OnAnyEnemyDied;

AStartechEnemyShip::AStartechEnemyShip()
{
	Team = ETeam::Enemy;

	Targetable = CreateDefaultSubobject<UTargetableComponent>(TEXT("Targetable"));
	Targetable->SetupAttachment(ShipMesh);

	Brain = CreateDefaultSubobject<UEnemyBrainComponent>(TEXT("Brain"));

	ChargedShot = CreateDefaultSubobject<UEnemyChargeComponent>(TEXT("ChargedShot"));

	MapMarker = CreateDefaultSubobject<UMapMarkerComponent>(TEXT("MapMarker"));
	MapMarker->SetupAttachment(ShipMesh);
	MapMarker->Type = EMapMarkerType::Enemy;
	MapMarker->bShowOnRadar = true;
	MapMarker->bShowScreenMarker = false;
}

void AStartechEnemyShip::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Also here so placed enemies show their mesh in the editor.
	ApplyMesh();
}

void AStartechEnemyShip::SetEnemyData(UEnemyData* const NewData)
{
	EnemyData = NewData;
}

void AStartechEnemyShip::BeginPlay()
{
	// Before Super, so components start play with the data already applied.
	ResetForNewLife();

	Super::BeginPlay();

	// Once per actor: the ammo stack it adds stays selected for every later life.
	ApplyAmmo();
}

void AStartechEnemyShip::OnAcquiredFromPool_Implementation()
{
	// The pool has already placed the ship, so its location is the new home.
	ResetForNewLife();
}

void AStartechEnemyShip::OnReleasedToPool_Implementation()
{
	Brain->StopBrain();
	StopWeapons();
	ResetFlightState();
	MapMarker->SetMarkerEnabled(false);
}

void AStartechEnemyShip::ResetForNewLife()
{
	// Before the mesh and stats: no charge glow left over, and the collision and materials the last death changed back.
	ChargedShot->CancelCharge();
	ShipDeath->ResetDeath();

	ApplyMesh();
	ApplyStats();

	// ApplyStats refills hull and shield; Revive also clears the dead flag and the shield regen delay.
	HealthComponent->Revive();

	ResetFlightState();
	// No afterburner left over from the last life.
	SetFlightScales(1.f, 1.f);
	StopWeapons();

	// The base HandleDeath switched both off.
	Targetable->SetTargetable(true);
	MapMarker->SetMarkerEnabled(true);

	Brain->ResetBrain(GetActorLocation());
}

void AStartechEnemyShip::StopWeapons()
{
	WeaponSystem->UpdateFiring(false, GetActorLocation(), 0.f);
	LaserMountLeft->ClearShots();
	LaserMountRight->ClearShots();
}

void AStartechEnemyShip::ApplyMesh()
{
	if (!EnemyData)
	{
		return;
	}

	if (EnemyData->Mesh)
	{
		ShipMesh->SetStaticMesh(EnemyData->Mesh);
	}
	ShipMesh->SetRelativeScale3D(FVector(EnemyData->MeshScale));
}

void AStartechEnemyShip::ApplyStats()
{
	if (!EnemyData)
	{
		return;
	}

	MaxForwardSpeed = EnemyData->MaxForwardSpeed;
	MaxStrafeSpeed = EnemyData->MaxStrafeSpeed;

	// The brain turns through facing at TurnRate; matching MaxTurnRate keeps the bank lean in proportion.
	MaxTurnRate = EnemyData->TurnRate;
	FacingTurnRate = EnemyData->TurnRate;

	LastDrop = FLootBundle();
	bLootPending = false;

	// The spawn tier, not the current one: the pool hands out enemies while the next sector generates.
	const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(this);
	LifeTier = Tiers ? Tiers->GetSpawnTier() : 1;
	const FSectorTierData Tier = Tiers ? Tiers->GetTierData(LifeTier) : FSectorTierData();

	// The type's combat numbers come from DA_CombatBalance; EnemyData's are the fallback for an unlisted type.
	const UCombatBalance* const Balance = UCombatBalance::Get();
	const FEnemyBalance* const Stats = Balance ? Balance->FindEnemy(EnemyData) : nullptr;
	if (!Stats && Balance)
	{
		// Once per type, not once per life.
		static TArray<FName> Warned;
		if (!Warned.Contains(EnemyData->GetFName()))
		{
			Warned.Add(EnemyData->GetFName());
			UE_LOG(LogTemp, Warning, TEXT("%s is not in DA_CombatBalance's Enemies; using its EnemyData stats."),
				*EnemyData->GetName());
		}
	}

	const float MaxHull = Stats ? Stats->Hull : EnemyData->MaxHull;
	const float MaxShield = Stats ? Stats->Shield : EnemyData->MaxShield;
	HealthComponent->InitializeHealth(MaxHull * Tier.EnemyHullMultiplier, MaxShield * Tier.EnemyShieldMultiplier);

	LifeLaserWeapon = Stats && Stats->LaserWeapon ? Stats->LaserWeapon.Get() : EnemyData->LaserWeapon.Get();
	LifePreferredRange = Stats ? Stats->PreferredRange : EnemyData->PreferredRange;
	LifeRewardMultiplier = Stats ? Stats->RewardMultiplier : 1.f;
	LifeBehaviour = Stats ? Stats->Behaviour : FEnemyBehaviourBalance();
	LifeDamageMultiplier = Tier.EnemyDamageMultiplier;
	LifeHomeLeashRange = LifeBehaviour.HomeLeashRange > 0.f ? LifeBehaviour.HomeLeashRange : EnemyData->LeashRange;
	// Some types keep pace with the player's Engines upgrade tier by tier.
	if (const float Speed = LifeBehaviour.GetForwardSpeed(LifeTier); Speed > 0.f)
	{
		MaxForwardSpeed = Speed;
	}

	ULaserWeaponData* const Laser = LifeLaserWeapon;
	LaserMountLeft->SetWeapon(Laser);
	LaserMountRight->SetWeapon(Laser);

	// A shot does weapon damage × ammo multiplier × scale; the scale makes it DamagePerShot × the tier's multiplier.
	const float NativeShot = Laser ? Laser->Damage * UCombatBalance::GetDamageMultiplier(EnemyData->LaserAmmo) : 0.f;
	const float ShotScale = Stats && NativeShot > 0.f ? Stats->DamagePerShot / NativeShot : 1.f;
	LaserMountLeft->SetDamageScale(Tier.EnemyDamageMultiplier * ShotScale);
	LaserMountRight->SetDamageScale(Tier.EnemyDamageMultiplier * ShotScale);
	ShieldDrainMount->SetDamageScale(Tier.EnemyDamageMultiplier);

	WeaponSystem->SetInfiniteAmmo(true);
}

void AStartechEnemyShip::ApplyAmmo()
{
	if (!EnemyData || !EnemyData->LaserAmmo)
	{
		return;
	}

	// The count never drops with infinite ammo; one round just gives the ammo a stack to select.
	WeaponSystem->AddAmmo(EnemyData->LaserAmmo, 1.f);
	WeaponSystem->SelectAmmo(EnemyData->LaserAmmo);
}

void AStartechEnemyShip::RollLoot()
{
	if (!EnemyData || !EnemyData->LootTable)
	{
		return;
	}

	// Credits use the kill multiplier of the tier this enemy spawned in.
	const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(this);
	FLootRollModifiers Modifiers = FLootRollModifiers::FromTier(
		Tiers ? Tiers->GetTierData(LifeTier) : FSectorTierData(), ELootRollSource::EnemyDrop);
	// Tougher types pay more: credits and amounts, not which items drop.
	Modifiers.CreditsMultiplier *= LifeRewardMultiplier;
	Modifiers.AmountMultiplier *= LifeRewardMultiplier;
	LastDrop = EnemyData->LootTable->Roll(Modifiers);
	// Tier ammo drops (LL4 in tiers III and IV) come on top of the table, as part of the same pickup.
	if (const UCombatBalance* const Balance = UCombatBalance::Get())
	{
		Balance->RollTierAmmoDrops(LifeTier, LastDrop);
	}
	bLootPending = true;
}

void AStartechEnemyShip::SpawnLoot(const FVector& Location)
{
	if (!bLootPending)
	{
		return;
	}
	bLootPending = false;

	if (!LootPickupClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has a loot table but no LootPickupClass; nothing dropped."), *GetName());
		return;
	}

	if (UDespawnSubsystem* const Despawner = GetWorld() ? GetWorld()->GetSubsystem<UDespawnSubsystem>() : nullptr)
	{
		Despawner->DropLoot(LootPickupClass, Location, LastDrop, LootLifespan);
	}
}

void AStartechEnemyShip::HandleDeath(AActor* const Killer)
{
	// Switches off the targetable, which releases the player's lock.
	Super::HandleDeath(Killer);

	// A telegraph in progress never fires; a shot already flying keeps flying.
	ChargedShot->CancelCharge();
	Brain->StopBrain();

	ClearFacingLocation();
	SetThrottleInput(0.f);
	SetTurnInput(0.f);
	SetStrafeInput(0.f);
	SetVerticalInput(0.f);
	WeaponSystem->UpdateFiring(false, GetActorLocation(), 0.f);

	// The rewards are decided now; the pickup appears when the wreck explodes.
	RollLoot();

	OnEnemyDied.Broadcast(this, Killer);
	OnAnyEnemyDied.Broadcast(*this, Killer);

	// Counts for quests only when Killer is the player ship.
	if (UStartechQuestBridge* const QuestBridge = UStartechQuestBridge::Get(this))
	{
		QuestBridge->ReportEnemyDestroyed(*this, Killer);
	}

}

void AStartechEnemyShip::HandleDyingFinished(const FVector& Location)
{
	Super::HandleDyingFinished(Location);

	SpawnLoot(Location);
	UDespawnSubsystem::Despawn(this);
}

const FShipDeathEffects& AStartechEnemyShip::GetDeathEffects() const
{
	return EnemyData ? EnemyData->DeathEffects : Super::GetDeathEffects();
}
