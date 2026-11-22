#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tiers/SectorTierTypes.h"
#include "StartechTierSubsystem.generated.h"

class USectorDirectorSubsystem;

/** Broadcast when the playable sector's tier changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSectorTierChanged, int32, NewTier);

/** Broadcast when the player first enters a tier above every tier visited before. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFirstVisitToTier, int32, Tier);

/**
 * The one place that answers "what tier is this sector, and what are its values" (UStartechSettings::SectorTiers and
 * TierData; nothing else reads those). An unknown sector counts as tier 1, with a warning logged once per sector.
 *
 * Two tiers matter:
 *   current tier  the playable sector's, switched on the director's OnSectorReady (OnTierChanged, and
 *                 OnFirstVisitToTier when it is above the highest tier the profile has visited)
 *   spawn tier    the tier of the sector being built or played; enemies and loot are taken from the pool while the
 *                 next sector generates, before the director's current sector switches, so they read this one
 */
UCLASS()
class STARTECH_API UStartechTierSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The tier subsystem of WorldContextObject's world, or null. */
	static UStartechTierSubsystem* Get(const UObject* WorldContextObject);

	/** Subscribes to the sector director, which is initialised first. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Unsubscribes from the sector director. */
	virtual void Deinitialize() override;

	/** The playable sector's tier; 1 before the first sector is ready. */
	UFUNCTION(BlueprintPure, Category = "Tiers")
	int32 GetCurrentTier() const { return CurrentTier; }

	/** The playable sector's tier values. */
	const FSectorTierData& GetCurrentTierData() const;

	/** The tier of the sector things spawn into now: the generating sector during a regeneration, else the current. */
	UFUNCTION(BlueprintPure, Category = "Tiers")
	int32 GetSpawnTier() const { return SpawnTier; }

	/** The spawn tier's values (enemy stats, loot). */
	const FSectorTierData& GetSpawnTierData() const;

	/** SectorId's tier, or 1 (warning once) if UStartechSettings::SectorTiers doesn't list it. */
	UFUNCTION(BlueprintPure, Category = "Tiers")
	int32 GetTierForSector(FName SectorId) const;

	/** Tier's values; a tier missing from UStartechSettings::TierData gives neutral values (all multipliers 1). */
	const FSectorTierData& GetTierData(int32 Tier) const;

	/**
	 * Tier's values without a world (the TTK table, tests): the entry of UStartechSettings::TierData, or null when
	 * it has none. In play, use GetTierData.
	 */
	static const FSectorTierData* FindTierData(int32 Tier);

	/** Fires on the director's OnSectorReady when the new sector's tier differs from the last one (and for the first). */
	UPROPERTY(BlueprintAssignable, Category = "Tiers")
	FOnSectorTierChanged OnTierChanged;

	/**
	 * Fires on the director's OnSectorReady, after OnTierChanged, when the sector's tier is above the profile's highest
	 * tier visited (UPlayerProfileSubsystem, saved in Startech.Profile), which it then raises. Never for tier 1.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Tiers")
	FOnFirstVisitToTier OnFirstVisitToTier;

protected:
	/** Game and Play-In-Editor worlds only, like the ProcSector subsystems it follows. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** A regeneration started: spawns now use ToSectorId's tier. */
	UFUNCTION()
	void HandleTransitionStarted(FName FromSectorId, FName ToSectorId);

	/** A sector is playable: it becomes the current tier. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** The director subscribed to, kept so Deinitialize can unsubscribe. */
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;

	/** The playable sector's tier. */
	int32 CurrentTier = 1;

	/** The tier spawns use. */
	int32 SpawnTier = 1;

	/** True once a sector has been ready, so the first one always fires OnTierChanged. */
	bool bHasSector = false;

	/** Sectors already warned about as missing from SectorTiers. */
	mutable TArray<FName> WarnedSectors;

	/** Tiers already warned about as missing from TierData. */
	mutable TArray<int32> WarnedTiers;
};
