#include "SaveSerialization.h"

#include "Misc/Crc.h"
#include "SaveFrameworkLog.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"

namespace SaveSerializationPrivate
{
	/** "SFS1": marks bytes written by FSaveSerialization. */
	static constexpr uint32 Magic = 0x53465331;

	/** Layout version of the envelope below. */
	static constexpr int32 EnvelopeVersion = 1;

	/** Bytes before the body: magic, envelope version, body size, body checksum. */
	static constexpr int32 PrefixSize = sizeof(uint32) + sizeof(int32) + sizeof(int32) + sizeof(uint32);

	/** Adds every custom version of From to Into. */
	static void MergeCustomVersions(FCustomVersionContainer& Into, const FCustomVersionContainer& From)
	{
		for (const FCustomVersion& Version : From.GetAllVersions())
		{
			Into.SetVersion(Version.Key, Version.Version, Version.GetFriendlyName());
		}
	}
}

/*
 * Layout: [Magic][EnvelopeVersion][BodySize][BodyCrc] then the body:
 * [UE4 file version][UE5 file version][licensee version][custom versions][payload], where the payload is the
 * tagged property stream. The versions let data written by an older engine be read with the rules it was written by.
 */
void FSaveSerialization::StructToBytes(const UScriptStruct* const Struct, const void* const Value, TArray<uint8>& OutData)
{
	using namespace SaveSerializationPrivate;

	OutData.Reset();
	if (!Struct || !Value)
	{
		UE_LOG(LogSaveFramework, Error, TEXT("StructToBytes: no struct or value."));
		return;
	}

	TArray<uint8> Payload;
	FMemoryWriter PayloadWriter(Payload, true);
	PayloadWriter.SetUEVer(GPackageFileUEVersion);
	PayloadWriter.SetLicenseeUEVer(GPackageFileLicenseeUEVersion);
	{
		FObjectAndNameAsStringProxyArchive Archive(PayloadWriter, false);
		Archive.ArIsSaveGame = true;
		// Saving never modifies the value; the engine API is simply not const.
		Struct->SerializeTaggedProperties(Archive, static_cast<uint8*>(const_cast<void*>(Value)), const_cast<UScriptStruct*>(Struct), nullptr);

		FCustomVersionContainer CustomVersions = PayloadWriter.GetCustomVersions();
		MergeCustomVersions(CustomVersions, Archive.GetCustomVersions());

		TArray<uint8> Body;
		FMemoryWriter BodyWriter(Body);
		FPackageFileVersion FileVersion = PayloadWriter.UEVer();
		int32 LicenseeVersion = PayloadWriter.LicenseeUEVer();
		BodyWriter << FileVersion.FileVersionUE4;
		BodyWriter << FileVersion.FileVersionUE5;
		BodyWriter << LicenseeVersion;
		CustomVersions.Serialize(BodyWriter);
		BodyWriter << Payload;

		uint32 MagicValue = Magic;
		int32 EnvelopeVersionValue = EnvelopeVersion;
		int32 BodySize = Body.Num();
		uint32 BodyCrc = FCrc::MemCrc32(Body.GetData(), Body.Num());

		FMemoryWriter OutWriter(OutData);
		OutWriter << MagicValue;
		OutWriter << EnvelopeVersionValue;
		OutWriter << BodySize;
		OutWriter << BodyCrc;
		OutData.Append(Body);
	}
}

bool FSaveSerialization::BytesToStruct(const TArray<uint8>& Data, const UScriptStruct* const Struct, void* const OutValue)
{
	using namespace SaveSerializationPrivate;

	if (!Struct || !OutValue)
	{
		UE_LOG(LogSaveFramework, Error, TEXT("BytesToStruct: no struct or value."));
		return false;
	}
	if (Data.Num() < PrefixSize)
	{
		return false;
	}

	FMemoryReader Reader(Data);
	uint32 MagicValue = 0;
	int32 EnvelopeVersionValue = 0;
	int32 BodySize = 0;
	uint32 BodyCrc = 0;
	Reader << MagicValue;
	Reader << EnvelopeVersionValue;
	Reader << BodySize;
	Reader << BodyCrc;
	if (MagicValue != Magic || EnvelopeVersionValue != EnvelopeVersion || BodySize != Data.Num() - PrefixSize)
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("BytesToStruct(%s): unknown or truncated data."), *Struct->GetName());
		return false;
	}
	if (FCrc::MemCrc32(Data.GetData() + PrefixSize, BodySize) != BodyCrc)
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("BytesToStruct(%s): checksum mismatch."), *Struct->GetName());
		return false;
	}

	FPackageFileVersion FileVersion;
	int32 LicenseeVersion = 0;
	FCustomVersionContainer CustomVersions;
	TArray<uint8> Payload;
	Reader << FileVersion.FileVersionUE4;
	Reader << FileVersion.FileVersionUE5;
	Reader << LicenseeVersion;
	CustomVersions.Serialize(Reader);
	Reader << Payload;
	if (Reader.IsError())
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("BytesToStruct(%s): malformed envelope."), *Struct->GetName());
		return false;
	}

	FMemoryReader PayloadReader(Payload, true);
	PayloadReader.SetUEVer(FileVersion);
	PayloadReader.SetLicenseeUEVer(LicenseeVersion);
	PayloadReader.SetCustomVersions(CustomVersions);

	// Read into a copy that starts from OutValue, so missing fields keep the caller's values and a failure changes nothing.
	FStructOnScope Scratch(Struct);
	Struct->CopyScriptStruct(Scratch.GetStructMemory(), OutValue);
	{
		FObjectAndNameAsStringProxyArchive Archive(PayloadReader, true);
		Archive.ArIsSaveGame = true;
		Struct->SerializeTaggedProperties(Archive, Scratch.GetStructMemory(), const_cast<UScriptStruct*>(Struct), nullptr);
		if (Archive.IsError() || PayloadReader.IsError())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("BytesToStruct(%s): the property stream could not be read."), *Struct->GetName());
			return false;
		}
	}
	Struct->CopyScriptStruct(OutValue, Scratch.GetStructMemory());
	return true;
}
