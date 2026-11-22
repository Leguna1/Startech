#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerProfileSubsystem.generated.h"

/** Broadcast when the player's callsign changes. The ship id is derived from it, so it may have changed too. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCallsignChanged, FText, NewCallsign);

/**
 * The player's identity for the whole game session: the callsign chosen at the start of a game, and a short ship id
 * derived from it. There is no save system yet; because the id is a stable hash of the callsign, the same name gives
 * the same id on every launch.
 */
UCLASS()
class STARTECH_API UPlayerProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The profile of the game instance World belongs to. Null without a game instance. */
	static UPlayerProfileSubsystem* Get(const UObject* WorldContextObject);

	/** The player's chosen name, or "Captain" until one is set. */
	UFUNCTION(BlueprintPure, Category = "Profile")
	FText GetCallsign() const { return Callsign; }

	/**
	 * Sets the callsign. Surrounding whitespace is trimmed; an empty name restores the default.
	 * Fires OnCallsignChanged only if the name actually changed. The hook the name-entry screen will call.
	 */
	UFUNCTION(BlueprintCallable, Category = "Profile")
	void SetCallsign(const FText& NewCallsign);

	/** "SN-" plus two digits from a stable hash of the callsign (case-insensitive), e.g. "SN-04". */
	UFUNCTION(BlueprintPure, Category = "Profile")
	FText GetShipId() const;

	/** The highest sector tier the player has entered in this game; 1 at the start (Terra). Saved in the profile. */
	UFUNCTION(BlueprintPure, Category = "Profile")
	int32 GetHighestTierVisited() const { return HighestTierVisited; }

	/** Sets the highest tier visited (at least 1). Called by the tier subsystem and the profile save. */
	void SetHighestTierVisited(int32 Tier) { HighestTierVisited = FMath::Max(Tier, 1); }

	/** Fires on every real change of the callsign. */
	UPROPERTY(BlueprintAssignable, Category = "Profile")
	FOnCallsignChanged OnCallsignChanged;

private:
	/** The name every profile starts with. */
	static FText GetDefaultCallsign();

	/** The current callsign. */
	FText Callsign = GetDefaultCallsign();

	/** See GetHighestTierVisited. */
	int32 HighestTierVisited = 1;
};
