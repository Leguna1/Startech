#include "HudFormat.h"

namespace HudFormat
{
	constexpr float CentimetresPerMetre = 100.f;
	constexpr float MetresPerKilometre = 1000.f;
}

FText FHudFormat::Distance(const float Centimetres)
{
	const float Metres = FMath::Max(Centimetres, 0.f) / HudFormat::CentimetresPerMetre;
	if (Metres < HudFormat::MetresPerKilometre)
	{
		return FText::Format(NSLOCTEXT("HudFormat", "Metres", "{0} m"), FText::AsNumber(FMath::RoundToInt(Metres)));
	}

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 1;
	return FText::Format(NSLOCTEXT("HudFormat", "Kilometres", "{0} km"),
		FText::AsNumber(Metres / HudFormat::MetresPerKilometre, &Options));
}

FText FHudFormat::Countdown(const float Seconds)
{
	const int32 Total = FMath::Max(FMath::CeilToInt(Seconds), 0);
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}
