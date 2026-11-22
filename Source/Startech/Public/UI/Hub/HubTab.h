#pragma once

#include "CoreMinimal.h"
#include "HubTab.generated.h"

/** A page of the station hub that can be opened. */
UENUM(BlueprintType)
enum class EHubTab : uint8
{
	/** View, accept, turn in and abandon quests; take bounties. */
	Quests,
	/** The station's shops and the Loadout tab (the existing shop screen). */
	Shop,
	/** Buy upgrade track levels (UHubUpgradesTabWidget). */
	Upgrades
};
