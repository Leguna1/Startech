#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BountyBoardSubsystem.generated.h"

class UBountyTemplate;
class UQuestDefinition;
class UQuestSubsystem;

/** Broadcast after the bounty board has been refreshed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBountyBoardRefreshed);

/**
 * The bounty board: a few generated bounty offers the player can accept. Offers are registered with UQuestSubsystem
 * as ordinary quests, so Accept, events and turn-in work on them unchanged. The board's only own state is the seed
 * counter, so the offers themselves are saved with the quest states and regenerated from template and seed.
 * The game decides when to refresh (for example, when the player visits the board).
 */
UCLASS()
class QUESTFRAMEWORK_API UBountyBoardSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Depends on UQuestSubsystem, and starts the seed counter at a random value so each new game differs. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Removes every offer that has not been accepted and generates new ones up to the settings' BountyBoardSize, each
	 * from a template picked with its seed, then broadcasts OnBoardRefreshed. Accepted bounties are kept.
	 */
	UFUNCTION(BlueprintCallable, Category = "Bounty")
	void Refresh();

	/** The current offers: generated quests that are registered and Available, oldest first. */
	UFUNCTION(BlueprintPure, Category = "Bounty")
	TArray<UQuestDefinition*> GetOffers() const;

	/** The seed the next generated offer will use. Save it with the quest states. */
	UFUNCTION(BlueprintPure, Category = "Bounty")
	int32 GetNextSeed() const { return NextSeed; }

	/** Restores the seed counter from a save, so offers after loading continue the saved sequence. */
	UFUNCTION(BlueprintCallable, Category = "Bounty")
	void SetNextSeed(int32 Seed) { NextSeed = Seed; }

	/** Fires after Refresh. */
	UPROPERTY(BlueprintAssignable, Category = "Bounty")
	FOnBountyBoardRefreshed OnBoardRefreshed;

private:
	/** The template an offer with this seed is generated from: picked from the settings' templates by the seed. */
	static const UBountyTemplate* PickTemplate(const TArray<const UBountyTemplate*>& Templates, int32 Seed);

	/** The quest subsystem the offers live in. */
	UQuestSubsystem* GetQuests() const;

	/** Seed of the next offer; increases by one per offer. */
	int32 NextSeed = 0;
};
