#include "SaveFrameworkSaveGame.h"

#include "Misc/Crc.h"

uint32 USaveFrameworkSaveGame::ComputeChecksum() const
{
	const int32 RecordCount = Records.Num();
	uint32 Crc = FCrc::MemCrc32(&Header.SlotIndex, sizeof(Header.SlotIndex));
	Crc = FCrc::MemCrc32(&RecordCount, sizeof(RecordCount), Crc);
	for (const FSaveRecord& Record : Records)
	{
		const FString Id = Record.ParticipantId.ToString();
		Crc = FCrc::MemCrc32(*Id, Id.Len() * sizeof(TCHAR), Crc);
		Crc = FCrc::MemCrc32(&Record.Version, sizeof(Record.Version), Crc);
		const int32 Size = Record.Data.Num();
		Crc = FCrc::MemCrc32(&Size, sizeof(Size), Crc);
		Crc = FCrc::MemCrc32(Record.Data.GetData(), Size, Crc);
	}
	return Crc == 0 ? 1 : Crc;
}

void USaveFrameworkSaveGame::Seal()
{
	Checksum = ComputeChecksum();
}

bool USaveFrameworkSaveGame::IsIntact() const
{
	return Header.FrameworkVersion >= 1 && Checksum == ComputeChecksum();
}

const FSaveRecord* USaveFrameworkSaveGame::FindRecord(const FName ParticipantId) const
{
	return Records.FindByPredicate([ParticipantId](const FSaveRecord& Record)
	{
		return Record.ParticipantId == ParticipantId;
	});
}
