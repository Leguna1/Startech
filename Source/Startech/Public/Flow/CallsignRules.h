#pragma once

#include "CoreMinimal.h"

/** The rule for a callsign typed on the new-game screen: 3 to 16 letters, digits and single spaces, after trimming. */
struct STARTECH_API FCallsignRules
{
	/** Fewest characters allowed. */
	static constexpr int32 MinLength = 3;

	/** Most characters allowed. */
	static constexpr int32 MaxLength = 16;

	/** Raw with surrounding whitespace removed. */
	static FString Normalize(const FString& Raw);

	/** True when Normalized has MinLength to MaxLength characters, each a letter, a digit or a single space. */
	static bool IsValid(const FString& Normalized);
};
