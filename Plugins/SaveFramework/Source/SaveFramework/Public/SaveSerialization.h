#pragma once

#include "CoreMinimal.h"

class UScriptStruct;

/**
 * Converts USTRUCTs to bytes and back with Unreal's tagged property serialization, restricted to SaveGame properties.
 *
 * - Fields are matched by name, so adding or removing a field never breaks old data: a removed field is skipped,
 *   a new field keeps the value the destination already holds (initialize the destination to defaults first).
 * - FName, FString, soft object paths and gameplay tags are stored as text and survive restarts.
 *   Members of nested structs need the SaveGame flag too (FGameplayTag and FGameplayTagContainer have it).
 * - The bytes carry the engine and custom versions they were written with, plus a checksum, so corrupt or
 *   truncated data is rejected (BytesToStruct returns false) instead of being read.
 */
struct SAVEFRAMEWORK_API FSaveSerialization
{
	/** Writes the SaveGame properties of Value into OutData (replacing its contents). */
	template <typename T>
	static void StructToBytes(const T& Value, TArray<uint8>& OutData)
	{
		StructToBytes(T::StaticStruct(), &Value, OutData);
	}

	/**
	 * Reads Data into OutValue. Fields missing from Data keep their current value in OutValue.
	 * Returns false, leaving OutValue unchanged, when Data is empty, corrupt or truncated.
	 */
	template <typename T>
	static bool BytesToStruct(const TArray<uint8>& Data, T& OutValue)
	{
		return BytesToStruct(Data, T::StaticStruct(), &OutValue);
	}

	/** Writes the SaveGame properties of Value (an instance of Struct) into OutData. */
	static void StructToBytes(const UScriptStruct* Struct, const void* Value, TArray<uint8>& OutData);

	/** Reads Data into OutValue (an instance of Struct). Returns false, leaving OutValue unchanged, on bad data. */
	static bool BytesToStruct(const TArray<uint8>& Data, const UScriptStruct* Struct, void* OutValue);
};
