#include "Save/UpgradesSaveParticipant.h"

#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"
#include "Upgrades/ShipUpgradeSubsystem.h"

const FName UUpgradesSaveParticipant::ParticipantId(TEXT("Startech.Upgrades"));

void UUpgradesSaveParticipant::Init(UShipUpgradeSubsystem& InUpgrades)
{
	Upgrades = &InUpgrades;
}

FName UUpgradesSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 UUpgradesSaveParticipant::GetSaveVersion() const
{
	return 1;
}

void UUpgradesSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	FStartechUpgradesSave Save;
	if (const UShipUpgradeSubsystem* const Subsystem = Upgrades.Get())
	{
		Save.Tracks = Subsystem->GetLevels();
	}
	FSaveSerialization::StructToBytes(Save, OutData);
}

void UUpgradesSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechUpgradesSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Upgrades: unreadable record; every track at 0 instead."));
		OnNewGame();
		return;
	}
	if (UShipUpgradeSubsystem* const Subsystem = Upgrades.Get())
	{
		Subsystem->RestoreLevels(Save.Tracks);
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Upgrades: %d track(s) above level 0 (version %d)."),
			Subsystem->GetLevels().Num(), SavedVersion);
	}
}

void UUpgradesSaveParticipant::OnNewGame()
{
	if (UShipUpgradeSubsystem* const Subsystem = Upgrades.Get())
	{
		Subsystem->ResetLevels();
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Upgrades: new game, every track at 0."));
	}
}
