#include "Data/ProcSectorSettings.h"

const UProcSectorSettings& UProcSectorSettings::Get()
{
	return *GetDefault<UProcSectorSettings>();
}

FName UProcSectorSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}
