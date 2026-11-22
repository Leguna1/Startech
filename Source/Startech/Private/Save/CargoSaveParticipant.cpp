#include "Save/CargoSaveParticipant.h"

#include "InventoryComponent.h"
#include "Save/StartechSaveConversion.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"

const FName UCargoSaveParticipant::ParticipantId(TEXT("Startech.Cargo"));

FName UCargoSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 UCargoSaveParticipant::GetSaveVersion() const
{
	return 1;
}

void UCargoSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	FStartechCargoSave Save;
	if (const UInventoryComponent* const Inventory = GetInventory())
	{
		Save.Items = FStartechSaveConversion::ToTagged(Inventory->GetItems(), TEXT("Startech.Cargo"));
	}
	FSaveSerialization::StructToBytes(Save, OutData);
}

void UCargoSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechCargoSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Cargo: unreadable record; empty hold instead."));
		OnNewGame();
		return;
	}
	if (UInventoryComponent* const Inventory = GetInventory())
	{
		Inventory->RestoreItems(FStartechSaveConversion::ToItemStacks(Save.Items, TEXT("Startech.Cargo")));
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Cargo: %d stack(s), %d / %d."), Inventory->GetItems().Num(),
			Inventory->GetCargoUsed(), Inventory->GetCargoCapacity());
	}
}

void UCargoSaveParticipant::OnNewGame()
{
	if (UInventoryComponent* const Inventory = GetInventory())
	{
		Inventory->RestoreItems(TArray<FItemStack>());
	}
}
