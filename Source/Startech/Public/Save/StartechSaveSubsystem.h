#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StartechSaveSubsystem.generated.h"

class UProfileSaveParticipant;
class UQuestsSaveParticipant;
class UUpgradesSaveParticipant;

/**
 * Startech's game-instance side of saving. When the game instance starts it creates and registers the participants
 * whose data lives for the whole session (Startech.Profile, Startech.Quests and Startech.Upgrades), so a slot loaded in the main menu
 * restores them before any game world exists. It also keeps the active slot's display name equal to the callsign.
 */
UCLASS()
class STARTECH_API UStartechSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of WorldContext's game instance, or null. */
	static UStartechSaveSubsystem* Get(const UObject* WorldContext);

	/** Creates and registers the Profile and Quests participants, and follows the callsign. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Unregisters the participants. */
	virtual void Deinitialize() override;

	/** The Quests participant, for listeners that must ignore the events a restore fires (IsRestoring). */
	const UQuestsSaveParticipant* GetQuestsParticipant() const { return QuestsParticipant; }

	/**
	 * The editor's test slot: SaveFramework's last slot (NumSlots - 1), above UStartechSettings::VisibleSaveSlots, so
	 * the menu and Continue never show it. Its display name always starts with TestSlotPrefix.
	 */
	static int32 GetTestSlotIndex();

	/** True while the active slot is the test slot. */
	bool IsTestSlotActive() const;

	/**
	 * Starts a temporary new game in the test slot, named "[PIE] <callsign>". For a game map started without the main
	 * menu (Play In Editor, or the map launched directly).
	 */
	void StartTestGame();

	/** "[PIE] ": the display-name prefix that marks the test slot. */
	static const FString TestSlotPrefix;

private:
	/** The active slot's display name for Callsign: the callsign, prefixed with TestSlotPrefix in the test slot. */
	FString MakeDisplayName(const FString& Callsign) const;

	/** Profile callback: the active slot shows the new callsign from its next save on. */
	UFUNCTION()
	void HandleCallsignChanged(FText NewCallsign);

	/** Startech.Profile. */
	UPROPERTY(Transient)
	TObjectPtr<UProfileSaveParticipant> ProfileParticipant;

	/** Startech.Quests. */
	UPROPERTY(Transient)
	TObjectPtr<UQuestsSaveParticipant> QuestsParticipant;

	/** Startech.Upgrades. */
	UPROPERTY(Transient)
	TObjectPtr<UUpgradesSaveParticipant> UpgradesParticipant;
};
