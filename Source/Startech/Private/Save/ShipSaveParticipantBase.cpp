#include "Save/ShipSaveParticipantBase.h"

#include "StartechShipPawn.h"

void UShipSaveParticipantBase::Init(AStartechShipPawn& InShip)
{
	OwnerShip = &InShip;
}

AStartechShipPawn* UShipSaveParticipantBase::GetShip() const
{
	return OwnerShip.Get();
}

UInventoryComponent* UShipSaveParticipantBase::GetInventory() const
{
	const AStartechShipPawn* const Owner = GetShip();
	return Owner ? Owner->GetInventory() : nullptr;
}

UWeaponSystemComponent* UShipSaveParticipantBase::GetWeaponSystem() const
{
	const AStartechShipPawn* const Owner = GetShip();
	return Owner ? Owner->GetWeaponSystem() : nullptr;
}
