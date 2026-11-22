#include "Upgrades/UpgradeCatalog.h"

float FUpgradeStatEffect::GetBonus(const int32 Level) const
{
	if (Level <= 0 || BonusAtLevel.Num() == 0)
	{
		return 0.f;
	}
	return BonusAtLevel[FMath::Min(Level, BonusAtLevel.Num()) - 1];
}

const FUpgradeTrack* UUpgradeCatalog::FindTrack(const FName TrackId) const
{
	return Tracks.FindByPredicate([TrackId](const FUpgradeTrack& Track) { return Track.TrackId == TrackId; });
}
