#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Death/ShipDeathTypes.h"
#include "EnemyBehavior.h"
#include "GameplayTagContainer.h"
#include "EnemyData.generated.h"

class UAmmoData;
class ULootTable;
class UStaticMesh;
class ULaserWeaponData;

// One enemy type: looks, stats and behaviour.
UCLASS(BlueprintType)
class STARTECH_API UEnemyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Identity

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// Uniform scale applied to the ship.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0.01"))
	float MeshScale = 1.f;

	/**
	 * What this enemy type is, for quests: the subject tags of its Quest.Event.TargetDestroyed event when the player
	 * destroys it. Give each type its own Enemy.<Type> tag (Enemy.Drifter, Enemy.Chaser, Enemy.Ranged).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Enemy"))
	FGameplayTagContainer EnemyTags;

	// Stats

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float MaxHull = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float MaxShield = 20.f;

	// cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float MaxForwardSpeed = 2200.f;

	// cm/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float MaxStrafeSpeed = 1200.f;

	// deg/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0"))
	float TurnRate = 70.f;

	// Fitted to both laser mounts. None means the enemy cannot shoot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<ULaserWeaponData> LaserWeapon = nullptr;

	// Loaded as the selected laser ammo, with infinite rounds. Its DamageMultiplier applies and it decides
	// how the shots look. None means plain shots with no effects.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UAmmoData> LaserAmmo = nullptr;

	// Behaviour

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour")
	EEnemyBehavior Behavior = EEnemyBehavior::Chaser;

	// Distance from this ship at which Chaser and Ranged notice the player, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0"))
	float AggroRange = 12000.f;

	// Gives up once the player is this far from the enemy's home, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0"))
	float LeashRange = 30000.f;

	// Distance the enemy tries to keep from the player while engaged, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0"))
	float PreferredRange = 3000.f;

	// Fires only when the player is within this many degrees of the nose.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", ClampMax = "180"))
	float FiringConeHalfAngle = 10.f;

	// Passive enemies wander within this distance of home, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0"))
	float WanderRadius = 4000.f;

	// Loot

	// Rolled at hull 0 and dropped as a pickup when the wreck explodes. None drops nothing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<ULootTable> LootTable = nullptr;

	// Death

	// This type's death effect hooks. Empty fields use UStartechSettings::Death.DefaultEffects.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Death")
	FShipDeathEffects DeathEffects;
};
