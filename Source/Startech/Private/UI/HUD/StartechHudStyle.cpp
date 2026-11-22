#include "UI/HUD/StartechHudStyle.h"

#include "Styling/CoreStyle.h"

const UStartechHudStyle& UStartechHudStyle::GetDefaultStyle()
{
	return *GetDefault<UStartechHudStyle>();
}

FLinearColor UStartechHudStyle::GetTierColor(const ETierColorRole Role) const
{
	switch (Role)
	{
	case ETierColorRole::Credits:
		return CreditsColor;
	case ETierColorRole::Hull:
		return HullColor;
	case ETierColorRole::Danger:
		return DangerColor;
	case ETierColorRole::Text:
	default:
		return TextColor;
	}
}

FSlateFontInfo UStartechHudStyle::MakeFont(const int32 Size, const bool bBold)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

FLinearColor UStartechHudStyle::FromHex(const TCHAR* const Hex, const float Alpha)
{
	// FLinearColor(FColor) converts from sRGB, so the colour on screen matches the hex value.
	FLinearColor Color(FColor::FromHex(Hex));
	Color.A = Alpha;
	return Color;
}
