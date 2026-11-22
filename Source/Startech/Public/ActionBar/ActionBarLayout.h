#pragma once

#include "CoreMinimal.h"
#include "ActionBar/ActionBarTypes.h"
#include "Engine/DataAsset.h"
#include "ActionBarLayout.generated.h"

/**
 * The action bar's slots in order (DA_ActionBarLayout, chosen in UStartechSettings::ActionBarLayout). Fixed for now;
 * a player-made layout would be another instance of the same data.
 */
UCLASS(BlueprintType)
class STARTECH_API UActionBarLayout : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The slots, left to right. Neighbouring slots with the same GroupLabel form one group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	TArray<FActionBarSlot> Slots;

	/** The index of the slot pressed by Action, or INDEX_NONE. */
	int32 FindSlotByAction(const UInputAction* Action) const;
};
