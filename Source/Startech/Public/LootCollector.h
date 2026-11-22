#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LootBundle.h"
#include "LootCollector.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class ULootCollector : public UInterface
{
	GENERATED_BODY()
};

// Anything that can pick up loot.
class STARTECH_API ILootCollector
{
	GENERATED_BODY()

public:
	// Takes what it can and returns whatever it could not take.
	virtual FLootBundle CollectLoot(const FLootBundle& Loot) = 0;
};
