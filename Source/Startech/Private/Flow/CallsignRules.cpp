#include "Flow/CallsignRules.h"

FString FCallsignRules::Normalize(const FString& Raw)
{
	return Raw.TrimStartAndEnd();
}

bool FCallsignRules::IsValid(const FString& Normalized)
{
	const int32 Length = Normalized.Len();
	if (Length < MinLength || Length > MaxLength)
	{
		return false;
	}

	TCHAR Previous = TEXT('\0');
	for (const TCHAR Char : Normalized)
	{
		const bool bSpace = Char == TEXT(' ');
		if (!bSpace && !FChar::IsAlpha(Char) && !FChar::IsDigit(Char))
		{
			return false;
		}
		if (bSpace && Previous == TEXT(' '))
		{
			return false;
		}
		Previous = Char;
	}
	// Trimmed, so the first and last characters are never spaces.
	return true;
}
