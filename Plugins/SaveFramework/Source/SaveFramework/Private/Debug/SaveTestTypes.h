#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "SaveTestTypes.generated.h"

/** Test only: a nested struct, to check that nested SaveGame members are kept. */
USTRUCT()
struct FSaveTestNested
{
	GENERATED_BODY()

	/** A nested value that is saved. */
	UPROPERTY(SaveGame)
	int32 NestedValue = 0;
};

/** Test only: the first version of the test participant's data. */
USTRUCT()
struct FSaveTestDataV1
{
	GENERATED_BODY()

	/** A number the console changes (Save.Test.Set). */
	UPROPERTY(SaveGame)
	int32 Counter = 0;

	/** Free text. */
	UPROPERTY(SaveGame)
	FString Note;

	/** A name, to check names survive. */
	UPROPERTY(SaveGame)
	FName Label;

	/** A soft path, to check asset references survive. */
	UPROPERTY(SaveGame)
	FSoftObjectPath Asset;

	/** A nested struct. */
	UPROPERTY(SaveGame)
	FSaveTestNested Nested;
};

/** Test only: the second version, which adds AddedField. Old (V1) data must still load into it. */
USTRUCT()
struct FSaveTestDataV2
{
	GENERATED_BODY()

	/** A number the console changes (Save.Test.Set). */
	UPROPERTY(SaveGame)
	int32 Counter = 0;

	/** Free text. */
	UPROPERTY(SaveGame)
	FString Note;

	/** A name, to check names survive. */
	UPROPERTY(SaveGame)
	FName Label;

	/** A soft path, to check asset references survive. */
	UPROPERTY(SaveGame)
	FSoftObjectPath Asset;

	/** A nested struct. */
	UPROPERTY(SaveGame)
	FSaveTestNested Nested;

	/** Added in version 2. Data written by version 1 leaves it at its default. */
	UPROPERTY(SaveGame)
	int32 AddedField = 42;

	/** Not SaveGame: must never be written. */
	UPROPERTY()
	int32 NotSaved = 0;
};
