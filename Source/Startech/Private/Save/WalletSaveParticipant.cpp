#include "Save/WalletSaveParticipant.h"

#include "InventoryComponent.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"
#include "StartechSettings.h"

const FName UWalletSaveParticipant::ParticipantId(TEXT("Startech.Wallet"));

FName UWalletSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 UWalletSaveParticipant::GetSaveVersion() const
{
	return 1;
}

void UWalletSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	FStartechWalletSave Save;
	if (const UInventoryComponent* const Inventory = GetInventory())
	{
		Save.Credits = Inventory->GetCredits();
	}
	FSaveSerialization::StructToBytes(Save, OutData);
}

void UWalletSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechWalletSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Wallet: unreadable record; new-game credits instead."));
		OnNewGame();
		return;
	}
	if (UInventoryComponent* const Inventory = GetInventory())
	{
		Inventory->RestoreCredits(Save.Credits);
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Wallet: %d credits."), Save.Credits);
	}
}

void UWalletSaveParticipant::OnNewGame()
{
	if (UInventoryComponent* const Inventory = GetInventory())
	{
		Inventory->RestoreCredits(UStartechSettings::Get().NewGameCredits);
	}
}
