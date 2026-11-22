#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SectorTierTypes.generated.h"

/** Which DA_HudStyle colour shows a tier, so the tier colours follow the style. */
UENUM(BlueprintType)
enum class ETierColorRole : uint8
{
	/** The style's TextColor. */
	Text,
	/** The style's CreditsColor. */
	Credits,
	/** The style's HullColor. */
	Hull,
	/** The style's DangerColor. */
	Danger
};

/** One sector's tier: which sector, and how far from home it is in difficulty. */
USTRUCT(BlueprintType)
struct STARTECH_API FSectorTierAssignment
{
	GENERATED_BODY()

	/** The ProcSector sector id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	FName SectorId;

	/** Its tier, from 1 (home); an index into UStartechSettings::TierData plus one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "1"))
	int32 Tier = 1;
};

/** How likely one mineral is when a loot roll picks a mineral in a tier. */
USTRUCT(BlueprintType)
struct STARTECH_API FTierMineralWeight
{
	GENERATED_BODY()

	/** The mineral, by its ItemTag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (Categories = "Item.Mineral"))
	FGameplayTag Mineral;

	/** Its weight against the other minerals of the tier; 0 never picks it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0"))
	float Weight = 0.f;
};

/** Everything that changes with a sector's tier: enemy toughness, rewards, loot and how the tier is shown. */
USTRUCT(BlueprintType)
struct STARTECH_API FSectorTierData
{
	GENERATED_BODY()

	/** The tier as shown: "I", "II". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier")
	FText DisplayName;

	/** Enemy max hull × this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemies", meta = (ClampMin = "0"))
	float EnemyHullMultiplier = 1.f;

	/** Enemy max shield × this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemies", meta = (ClampMin = "0"))
	float EnemyShieldMultiplier = 1.f;

	/** Damage dealt by enemies × this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemies", meta = (ClampMin = "0"))
	float EnemyDamageMultiplier = 1.f;

	/** Credits in an enemy's drop × this; also the bounty reward multiplier for the tier's sectors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
	float KillCreditsMultiplier = 1.f;

	/** Mineral and ammo amounts in every loot roll, and credits in world loot, × this (rounded, at least 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards", meta = (ClampMin = "0"))
	float LootAmountMultiplier = 1.f;

	/** Which mineral a loot roll gives, by weight. Empty keeps the loot table's own minerals. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rewards")
	TArray<FTierMineralWeight> MineralWeights;

	/** The DA_HudStyle colour the tier is shown in on the sector banner and gate markers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	ETierColorRole DangerColor = ETierColorRole::Text;

	/** Shown in the quest notice area the first time the player enters this tier. Empty shows nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText FirstVisitNotice;

	/** The DA_HudStyle colour of FirstVisitNotice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	ETierColorRole NoticeColor = ETierColorRole::Hull;
};
