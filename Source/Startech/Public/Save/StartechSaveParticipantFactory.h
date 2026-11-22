#pragma once

#include "CoreMinimal.h"

class AStartechShipPawn;
class UBountyBoardSubsystem;
class UPlayerProfileSubsystem;
class UProfileSaveParticipant;
class UQuestSubsystem;
class UQuestsSaveParticipant;
class USectorDirectorSubsystem;
class USectorsSaveParticipant;
class UShipUpgradeSubsystem;
class UUpgradesSaveParticipant;

/**
 * Creates every Startech save participant, ready to register with USaveSubsystem. The ids are fixed by the classes
 * (Startech.Profile, .Wallet, .Cargo, .Arsenal, .Quests, .Sectors, .Upgrades) and listed in SaveFramework's ExpectedParticipantIds.
 */
struct STARTECH_API FStartechSaveParticipantFactory
{
	/** The participants that live on the player's ship: Wallet, Cargo and Arsenal, with the ship as their outer. */
	static TArray<UObject*> CreateShipParticipants(AStartechShipPawn& Ship);

	/** Startech.Profile for Profile, owned by Outer. */
	static UProfileSaveParticipant* CreateProfile(UObject& Outer, UPlayerProfileSubsystem& Profile);

	/** Startech.Quests for Quests and Board, owned by Outer. */
	static UQuestsSaveParticipant* CreateQuests(UObject& Outer, UQuestSubsystem& Quests, UBountyBoardSubsystem& Board);

	/** Startech.Sectors for Director, owned by Outer. */
	static USectorsSaveParticipant* CreateSectors(UObject& Outer, USectorDirectorSubsystem& Director);

	/** Startech.Upgrades for Upgrades, owned by Outer. */
	static UUpgradesSaveParticipant* CreateUpgrades(UObject& Outer, UShipUpgradeSubsystem& Upgrades);
};
