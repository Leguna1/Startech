#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ShipSaveParticipantBase.generated.h"

class AStartechShipPawn;
class UInventoryComponent;
class UWeaponSystemComponent;

/**
 * Base of the participants that live on the player's ship (Wallet, Cargo, Arsenal): access to the ship and its
 * components. Each subclass implements ISaveParticipant itself. Each is created by
 * FStartechSaveParticipantFactory with the ship as its outer, registered in AStartechShipPawn::BeginPlay (after the
 * components' BeginPlay set up the Blueprint defaults) and unregistered in EndPlay.
 */
UCLASS(Abstract)
class STARTECH_API UShipSaveParticipantBase : public UObject
{
	GENERATED_BODY()

public:
	/** Remembers the ship this participant saves. */
	void Init(AStartechShipPawn& InShip);

protected:
	/** The ship, or null once it is gone. */
	AStartechShipPawn* GetShip() const;

	/** The ship's inventory, or null. */
	UInventoryComponent* GetInventory() const;

	/** The ship's weapon system, or null. */
	UWeaponSystemComponent* GetWeaponSystem() const;

private:
	/** The ship, held weakly: the ship owns this object, not the other way round. */
	TWeakObjectPtr<AStartechShipPawn> OwnerShip;
};
