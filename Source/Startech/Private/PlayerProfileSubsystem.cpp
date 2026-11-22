#include "PlayerProfileSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Crc.h"
#include "StartechHUDLog.h"

UPlayerProfileSubsystem* UPlayerProfileSubsystem::Get(const UObject* const WorldContextObject)
{
	const UWorld* const World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UPlayerProfileSubsystem>() : nullptr;
}

FText UPlayerProfileSubsystem::GetDefaultCallsign()
{
	return NSLOCTEXT("PlayerProfile", "DefaultCallsign", "Captain");
}

void UPlayerProfileSubsystem::SetCallsign(const FText& NewCallsign)
{
	const FString Trimmed = NewCallsign.ToString().TrimStartAndEnd();
	const FText Resolved = Trimmed.IsEmpty() ? GetDefaultCallsign() : FText::FromString(Trimmed);
	if (Resolved.ToString().Equals(Callsign.ToString(), ESearchCase::CaseSensitive))
	{
		return;
	}

	Callsign = Resolved;
	STARTECH_HUD_LOG(TEXT("OnCallsignChanged: '%s' (ship id %s)"), *Callsign.ToString(), *GetShipId().ToString());
	OnCallsignChanged.Broadcast(Callsign);
}

FText UPlayerProfileSubsystem::GetShipId() const
{
	// CRC32 of the upper-cased name: stable across launches, platforms and engine versions, unlike GetTypeHash.
	const uint32 Hash = FCrc::StrCrc32(*Callsign.ToString().ToUpper());
	return FText::FromString(FString::Printf(TEXT("SN-%02u"), Hash % 100u));
}
