#include "Save/StartechSectorSaveSubsystem.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/SaveSubsystem.h"
#include "Save/SectorsSaveParticipant.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveParticipantFactory.h"

void UStartechSectorSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	USectorDirectorSubsystem* const Director = Collection.InitializeDependency<USectorDirectorSubsystem>();
	Super::Initialize(Collection);

	if (!Director)
	{
		return;
	}
	Participant = FStartechSaveParticipantFactory::CreateSectors(*this, *Director);
	TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UStartechSectorSaveSubsystem::HandleWorldBeginTearDown);
	TryRegister();
}

void UStartechSectorSaveSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	TryRegister();
}

void UStartechSectorSaveSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
	Unregister();
	Participant = nullptr;
	Super::Deinitialize();
}

bool UStartechSectorSaveSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UStartechSectorSaveSubsystem::TryRegister()
{
	if (bRegistered || !Participant)
	{
		return;
	}
	const UGameInstance* const GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (USaveSubsystem* const Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr)
	{
		Saves->RegisterParticipant(Participant);
		bRegistered = true;
	}
}

void UStartechSectorSaveSubsystem::Unregister()
{
	if (!bRegistered)
	{
		return;
	}
	const UGameInstance* const GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (USaveSubsystem* const Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr)
	{
		Saves->UnregisterParticipant(Participant);
	}
	bRegistered = false;
}

void UStartechSectorSaveSubsystem::HandleWorldBeginTearDown(UWorld* const World)
{
	if (World == GetWorld())
	{
		Unregister();
	}
}
