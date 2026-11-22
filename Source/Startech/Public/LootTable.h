#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LootBundle.h"
#include "Tiers/SectorTierTypes.h"
#include "LootTable.generated.h"

class UAmmoData;
class UItemData;

USTRUCT(BlueprintType)
struct STARTECH_API FLootItemEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UItemData> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxCount = 1;
};

USTRUCT(BlueprintType)
struct STARTECH_API FLootAmmoEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UAmmoData> Ammo = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxCount = 1;
};

/** Where a loot roll happens, which decides the tier multiplier its credits use. */
enum class ELootRollSource : uint8
{
	/** Zone loot and anything else lying in the world: credits × LootAmountMultiplier. */
	World,
	/** A destroyed enemy's drop: credits × KillCreditsMultiplier. */
	EnemyDrop
};

/** How a sector's tier changes one loot roll. The defaults change nothing. */
struct STARTECH_API FLootRollModifiers
{
	/** Rolled credits × this. */
	float CreditsMultiplier = 1.f;

	/** Rolled item and ammo counts × this. */
	float AmountMultiplier = 1.f;

	/** When any has weight, minerals come from one weighted roll instead of one roll per mineral entry. */
	TArray<FTierMineralWeight> MineralWeights;

	/** The modifiers of Tier for a roll made at Source. */
	static FLootRollModifiers FromTier(const FSectorTierData& Tier, ELootRollSource Source);
};

/**
 * What a source can drop. The credits, every ammo entry and every non-mineral item entry are rolled independently.
 * Minerals: when the roll's modifiers have mineral weights (the sector's tier), one roll at MineralChance gives one
 * mineral picked by those weights, in the amount of that mineral's own entry; without weights each mineral entry is
 * rolled on its own. Every rolled amount is scaled by the modifiers, rounded, and at least 1.
 */
UCLASS(BlueprintType)
class STARTECH_API ULootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** One roll with no tier changes. */
	FLootBundle Roll() const;

	/** One roll changed by Modifiers. Can come out empty. */
	FLootBundle Roll(const FLootRollModifiers& Modifiers) const;

	/**
	 * Chance of the one mineral roll when the tier weights minerals. The mineral is picked among the minerals that
	 * have an entry in Items and weight in the tier; that entry's Min/MaxCount is the amount, its Chance unused.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items", meta = (ClampMin = "0", ClampMax = "1"))
	float MineralChance = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits", meta = (ClampMin = "0", ClampMax = "1"))
	float CreditChance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits", meta = (ClampMin = "0"))
	int32 CreditsMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits", meta = (ClampMin = "0"))
	int32 CreditsMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	TArray<FLootItemEntry> Items;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	TArray<FLootAmmoEntry> Ammo;
};
