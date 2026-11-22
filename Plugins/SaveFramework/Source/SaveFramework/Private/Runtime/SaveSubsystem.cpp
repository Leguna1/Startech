#include "Runtime/SaveSubsystem.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Factories/SaveGameFactory.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "SaveFrameworkLog.h"
#include "SaveFrameworkSaveGame.h"
#include "SaveFrameworkSettings.h"
#include "SaveParticipant.h"
#include "Storage/SaveSlotStorage.h"

#define LOCTEXT_NAMESPACE "SaveSubsystem"

namespace SaveSubsystemPrivate
{
	/** Longest wait for an asynchronous save before a synchronous one goes ahead anyway. */
	static constexpr double MaxSaveWaitSeconds = 5.0;
}

USaveSubsystem* USaveSubsystem::Get(const UObject* const WorldContext)
{
	const UWorld* const World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

void USaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveSlotIndex = INDEX_NONE;
	bSlotInfosRead = false;
}

void USaveSubsystem::Deinitialize()
{
	if (HasActiveSlot() && USaveFrameworkSettings::Get().bSaveOnShutdown)
	{
		SaveNow(TEXT("Shutdown"));
	}
	else
	{
		WaitForSaveInFlight();
	}
	CancelPendingSave();
	Participants.Reset();
	Super::Deinitialize();
}

// ---- Tickable ----

void USaveSubsystem::Tick(const float DeltaTime)
{
	ActiveHeader.PlayTimeSeconds += DeltaTime;
}

TStatId USaveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USaveSubsystem, STATGROUP_Tickables);
}

ETickableTickType USaveSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool USaveSubsystem::IsTickable() const
{
	return HasActiveSlot();
}

UWorld* USaveSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

// ---- Slots ----

TArray<FSaveSlotInfo> USaveSubsystem::GetSlotInfos() const
{
	EnsureSlotInfos();
	return SlotInfos;
}

void USaveSubsystem::RefreshSlotInfos()
{
	bSlotInfosRead = false;
	EnsureSlotInfos();
	OnSlotsChanged.Broadcast();
}

bool USaveSubsystem::CreateNewGame(const int32 SlotIndex, const FString& DisplayName)
{
	if (!USaveFrameworkSettings::Get().IsValidSlotIndex(SlotIndex))
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("CreateNewGame: slot %d is out of range."), SlotIndex);
		return false;
	}

	const FSaveSlotHeader Header = FSaveGameFactory::MakeHeader(SlotIndex, DisplayName, 0.0);
	UE_LOG(LogSaveFramework, Log, TEXT("New game in slot %d (\"%s\")."), SlotIndex, *DisplayName);
	ActivateSlot(SlotIndex, Header, TArray<FSaveRecord>(), false);
	return true;
}

bool USaveSubsystem::LoadSlot(const int32 SlotIndex, FText& OutReason)
{
	if (!USaveFrameworkSettings::Get().IsValidSlotIndex(SlotIndex))
	{
		OutReason = FText::Format(LOCTEXT("OutOfRange", "Slot {0} does not exist."), SlotIndex);
		return false;
	}

	USaveFrameworkSaveGame* SaveGame = nullptr;
	const ESaveSlotLoadResult Result = FSaveSlotStorage::LoadWithFallback(GetSlotName(SlotIndex), SaveGame);
	switch (Result)
	{
	case ESaveSlotLoadResult::Empty:
		OutReason = LOCTEXT("Empty", "The slot is empty.");
		return false;
	case ESaveSlotLoadResult::Corrupt:
		OutReason = LOCTEXT("Corrupt", "The save and its backup are both damaged.");
		return false;
	case ESaveSlotLoadResult::Loaded:
	case ESaveSlotLoadResult::RestoredFromBackup:
		break;
	}

	FSaveSlotHeader Header = SaveGame->Header;
	Header.SlotIndex = SlotIndex;
	const bool bRestored = Result == ESaveSlotLoadResult::RestoredFromBackup;
	UE_LOG(LogSaveFramework, Log, TEXT("Loaded slot %d (\"%s\", %d record(s))%s."), SlotIndex, *Header.DisplayName,
		SaveGame->Records.Num(), bRestored ? TEXT(" from its backup") : TEXT(""));
	ActivateSlot(SlotIndex, Header, SaveGame->Records, bRestored);
	OutReason = FText::GetEmpty();
	return true;
}

bool USaveSubsystem::DeleteSlot(const int32 SlotIndex)
{
	if (!USaveFrameworkSettings::Get().IsValidSlotIndex(SlotIndex))
	{
		return false;
	}
	if (SlotIndex == ActiveSlotIndex)
	{
		CloseActiveSlot();
	}
	WaitForSaveInFlight();

	const bool bDeleted = FSaveSlotStorage::DeleteSlot(GetSlotName(SlotIndex));
	UE_LOG(LogSaveFramework, Log, TEXT("Deleted slot %d%s."), SlotIndex, bDeleted ? TEXT("") : TEXT(" (partly failed)"));

	EnsureSlotInfos();
	if (SlotInfos.IsValidIndex(SlotIndex))
	{
		SlotInfos[SlotIndex] = ReadSlotInfo(SlotIndex);
	}
	OnSlotsChanged.Broadcast();
	return bDeleted;
}

int32 USaveSubsystem::GetActiveSlot() const
{
	return ActiveSlotIndex;
}

bool USaveSubsystem::HasActiveSlot() const
{
	return ActiveSlotIndex != INDEX_NONE;
}

void USaveSubsystem::CloseActiveSlot()
{
	if (!HasActiveSlot())
	{
		return;
	}
	UE_LOG(LogSaveFramework, Log, TEXT("Closed slot %d."), ActiveSlotIndex);
	CancelPendingSave();
	bSaveQueued = false;
	ActiveSlotIndex = INDEX_NONE;
	ActiveHeader = FSaveSlotHeader();
	bActiveRestoredFromBackup = false;
	KnownRecords.Reset();
	LoadedParticipantIds.Reset();
	bAllParticipantsLoadedFired = false;
}

FSaveSlotHeader USaveSubsystem::GetActiveHeader() const
{
	return ActiveHeader;
}

void USaveSubsystem::SetActiveDisplayName(const FString& DisplayName)
{
	if (HasActiveSlot())
	{
		ActiveHeader.DisplayName = DisplayName;
	}
}

bool USaveSubsystem::WasActiveSlotRestoredFromBackup() const
{
	return bActiveRestoredFromBackup;
}

FString USaveSubsystem::GetSlotName(const int32 SlotIndex) const
{
	return USaveFrameworkSettings::Get().MakeSlotName(SlotIndex);
}

void USaveSubsystem::ActivateSlot(const int32 SlotIndex, const FSaveSlotHeader& Header, const TArray<FSaveRecord>& Records,
	const bool bRestoredFromBackup)
{
	CancelPendingSave();
	bSaveQueued = false;

	ActiveSlotIndex = SlotIndex;
	ActiveHeader = Header;
	bActiveRestoredFromBackup = bRestoredFromBackup;
	KnownRecords = Records;
	LoadedParticipantIds.Reset();
	bAllParticipantsLoadedFired = false;

	// A participant may register others while reading, so walk a copy.
	const TArray<FRegisteredParticipant> Snapshot = Participants;
	for (const FRegisteredParticipant& Entry : Snapshot)
	{
		if (ISaveParticipant* const Participant = Cast<ISaveParticipant>(Entry.Object.Get()))
		{
			ApplyToParticipant(*Participant, Entry.Id);
		}
	}

	OnSlotLoaded.Broadcast(SlotIndex);
	CheckAllParticipantsLoaded();
}

// ---- Participants ----

void USaveSubsystem::RegisterParticipant(UObject* const Participant)
{
	ISaveParticipant* const AsParticipant = Cast<ISaveParticipant>(Participant);
	if (!AsParticipant)
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("RegisterParticipant: %s does not implement ISaveParticipant."), *GetNameSafe(Participant));
		return;
	}
	if (FindParticipantIndex(Participant) != INDEX_NONE)
	{
		return;
	}

	const FName Id = AsParticipant->GetSaveParticipantId();
	if (Id.IsNone())
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("RegisterParticipant: %s has no participant id."), *GetNameSafe(Participant));
		return;
	}
	const FRegisteredParticipant* const SameId = Participants.FindByPredicate([Id](const FRegisteredParticipant& Entry)
	{
		return Entry.Id == Id && Entry.Object.IsValid();
	});
	if (SameId)
	{
		UE_LOG(LogSaveFramework, Error, TEXT("RegisterParticipant: id '%s' is already used by %s; %s is not registered."),
			*Id.ToString(), *GetNameSafe(SameId->Object.Get()), *GetNameSafe(Participant));
		return;
	}

	FRegisteredParticipant& Entry = Participants.AddDefaulted_GetRef();
	Entry.Object = Participant;
	Entry.Id = Id;
	UE_LOG(LogSaveFramework, Verbose, TEXT("Registered participant '%s' (%s)."), *Id.ToString(), *GetNameSafe(Participant));

	if (HasActiveSlot())
	{
		ApplyToParticipant(*AsParticipant, Id);
		CheckAllParticipantsLoaded();
	}
}

void USaveSubsystem::UnregisterParticipant(UObject* const Participant)
{
	const int32 Index = FindParticipantIndex(Participant);
	if (Index == INDEX_NONE)
	{
		return;
	}

	// Keep what it has now, so the next save still writes it.
	if (HasActiveSlot())
	{
		if (const ISaveParticipant* const AsParticipant = Cast<ISaveParticipant>(Participant))
		{
			StoreKnownRecord(FSaveGameFactory::MakeRecord(*AsParticipant));
		}
	}
	UE_LOG(LogSaveFramework, Verbose, TEXT("Unregistered participant '%s'."), *Participants[Index].Id.ToString());
	Participants.RemoveAt(Index);
}

bool USaveSubsystem::IsParticipantRegistered(const UObject* const Participant) const
{
	return FindParticipantIndex(Participant) != INDEX_NONE;
}

TArray<FName> USaveSubsystem::GetRegisteredParticipantIds() const
{
	TArray<FName> Ids;
	for (const FRegisteredParticipant& Entry : Participants)
	{
		if (Entry.Object.IsValid())
		{
			Ids.Add(Entry.Id);
		}
	}
	return Ids;
}

TArray<FName> USaveSubsystem::GetPendingParticipantIds() const
{
	const TArray<FName> Registered = GetRegisteredParticipantIds();
	TArray<FName> Pending;
	for (const FSaveRecord& Record : KnownRecords)
	{
		if (!Registered.Contains(Record.ParticipantId))
		{
			Pending.Add(Record.ParticipantId);
		}
	}
	return Pending;
}

void USaveSubsystem::ApplyToParticipant(ISaveParticipant& Participant, const FName Id)
{
	const FSaveRecord* const Record = KnownRecords.FindByPredicate([Id](const FSaveRecord& Each)
	{
		return Each.ParticipantId == Id;
	});
	if (Record)
	{
		const int32 CurrentVersion = Participant.GetSaveVersion();
		if (Record->Version > CurrentVersion)
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("'%s': record version %d is newer than the code's version %d."),
				*Id.ToString(), Record->Version, CurrentVersion);
		}
		// Copy first: the participant may cause KnownRecords to change while reading.
		const FSaveRecord Copy = *Record;
		Participant.ReadSaveData(Copy.Data, Copy.Version);
	}
	else
	{
		Participant.OnNewGame();
	}
	LoadedParticipantIds.AddUnique(Id);
}

void USaveSubsystem::CheckAllParticipantsLoaded()
{
	if (bAllParticipantsLoadedFired || !HasActiveSlot())
	{
		return;
	}
	for (const FName Expected : USaveFrameworkSettings::Get().ExpectedParticipantIds)
	{
		if (!LoadedParticipantIds.Contains(Expected))
		{
			return;
		}
	}
	bAllParticipantsLoadedFired = true;
	UE_LOG(LogSaveFramework, Log, TEXT("All expected participants of slot %d are loaded."), ActiveSlotIndex);
	OnAllParticipantsLoaded.Broadcast();
}

void USaveSubsystem::StoreKnownRecord(FSaveRecord&& Record)
{
	const FName Id = Record.ParticipantId;
	FSaveRecord* const Existing = KnownRecords.FindByPredicate([Id](const FSaveRecord& Each)
	{
		return Each.ParticipantId == Id;
	});
	if (Existing)
	{
		*Existing = MoveTemp(Record);
	}
	else
	{
		KnownRecords.Add(MoveTemp(Record));
	}
}

void USaveSubsystem::CollectRecords()
{
	Participants.RemoveAll([](const FRegisteredParticipant& Entry)
	{
		if (!Entry.Object.IsValid())
		{
			UE_LOG(LogSaveFramework, Warning, TEXT("Participant '%s' was destroyed without unregistering; its last saved data is kept."),
				*Entry.Id.ToString());
			return true;
		}
		return false;
	});

	for (const FRegisteredParticipant& Entry : Participants)
	{
		if (const ISaveParticipant* const Participant = Cast<ISaveParticipant>(Entry.Object.Get()))
		{
			StoreKnownRecord(FSaveGameFactory::MakeRecord(*Participant));
		}
	}
}

int32 USaveSubsystem::FindParticipantIndex(const UObject* const Object) const
{
	if (!Object)
	{
		return INDEX_NONE;
	}
	return Participants.IndexOfByPredicate([Object](const FRegisteredParticipant& Entry)
	{
		return Entry.Object.Get() == Object;
	});
}

// ---- Saving ----

void USaveSubsystem::RequestSave(const FName Reason)
{
	if (!HasActiveSlot())
	{
		UE_LOG(LogSaveFramework, Verbose, TEXT("RequestSave(%s) ignored: no active slot."), *Reason.ToString());
		return;
	}

	PendingSaveReason = Reason;
	if (PendingSaveHandle.IsValid())
	{
		return; // Merged into the pending save.
	}
	const float Delay = FMath::Max(0.f, USaveFrameworkSettings::Get().AutosaveDebounceSeconds);
	PendingSaveHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USaveSubsystem::HandleDebounceElapsed), Delay);
}

bool USaveSubsystem::HandleDebounceElapsed(const float DeltaTime)
{
	PendingSaveHandle.Reset();
	if (!HasActiveSlot())
	{
		return false;
	}
	if (bSaveInFlight)
	{
		bSaveQueued = true;
		QueuedSaveReason = PendingSaveReason;
		return false;
	}
	StartAsyncSave(PendingSaveReason);
	return false;
}

void USaveSubsystem::CancelPendingSave()
{
	if (PendingSaveHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingSaveHandle);
		PendingSaveHandle.Reset();
	}
}

FSaveSlotHeader USaveSubsystem::MakeHeaderForSave() const
{
	return FSaveGameFactory::MakeHeader(ActiveSlotIndex, ActiveHeader.DisplayName, ActiveHeader.PlayTimeSeconds);
}

void USaveSubsystem::StartAsyncSave(const FName Reason)
{
	OnSaveStarted.Broadcast(Reason);
	CollectRecords();

	const FSaveSlotHeader Header = MakeHeaderForSave();
	USaveFrameworkSaveGame* const SaveGame = FSaveGameFactory::MakeSaveGame(Header, KnownRecords);
	bSaveInFlight = true;
	UE_LOG(LogSaveFramework, Log, TEXT("Saving slot %d (%s, %d record(s))..."), Header.SlotIndex, *Reason.ToString(), KnownRecords.Num());

	const TWeakObjectPtr<USaveSubsystem> WeakThis(this);
	FSaveSlotStorage::SaveAsync(*SaveGame, GetSlotName(Header.SlotIndex), [WeakThis, Header, Reason](const bool bSuccess)
	{
		if (USaveSubsystem* const Self = WeakThis.Get())
		{
			Self->HandleAsyncSaveCompleted(bSuccess, Header, Reason);
		}
	});
}

void USaveSubsystem::HandleAsyncSaveCompleted(const bool bSuccess, const FSaveSlotHeader& SavedHeader, const FName Reason)
{
	bSaveInFlight = false;
	UE_LOG(LogSaveFramework, Log, TEXT("Save of slot %d (%s) %s."), SavedHeader.SlotIndex, *Reason.ToString(),
		bSuccess ? TEXT("completed") : TEXT("FAILED"));
	if (bSuccess)
	{
		UpdateSlotInfo(SavedHeader);
	}
	OnSaveCompleted.Broadcast(bSuccess, Reason);

	if (bSaveQueued)
	{
		bSaveQueued = false;
		if (HasActiveSlot())
		{
			StartAsyncSave(QueuedSaveReason);
		}
	}
}

bool USaveSubsystem::SaveNow(const FName Reason)
{
	if (!HasActiveSlot())
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("SaveNow(%s): no active slot."), *Reason.ToString());
		return false;
	}

	CancelPendingSave();
	WaitForSaveInFlight();
	bSaveQueued = false; // This save covers it.

	OnSaveStarted.Broadcast(Reason);
	CollectRecords();
	const FSaveSlotHeader Header = MakeHeaderForSave();
	USaveFrameworkSaveGame* const SaveGame = FSaveGameFactory::MakeSaveGame(Header, KnownRecords);
	const bool bSuccess = FSaveSlotStorage::SaveSync(*SaveGame, GetSlotName(Header.SlotIndex));
	UE_LOG(LogSaveFramework, Log, TEXT("Saved slot %d now (%s, %d record(s)) %s."), Header.SlotIndex, *Reason.ToString(),
		KnownRecords.Num(), bSuccess ? TEXT("successfully") : TEXT("- FAILED"));
	if (bSuccess)
	{
		UpdateSlotInfo(Header);
	}
	OnSaveCompleted.Broadcast(bSuccess, Reason);
	return bSuccess;
}

bool USaveSubsystem::IsSaveInFlight() const
{
	return bSaveInFlight;
}

void USaveSubsystem::WaitForSaveInFlight()
{
	if (!bSaveInFlight)
	{
		return;
	}
	const double GiveUpAt = FPlatformTime::Seconds() + SaveSubsystemPrivate::MaxSaveWaitSeconds;
	while (bSaveInFlight && FPlatformTime::Seconds() < GiveUpAt)
	{
		// The completion callback is a game-thread task.
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FPlatformProcess::Sleep(0.005f);
	}
	if (bSaveInFlight)
	{
		UE_LOG(LogSaveFramework, Warning, TEXT("The asynchronous save did not finish within %.0f s; continuing."),
			SaveSubsystemPrivate::MaxSaveWaitSeconds);
		bSaveInFlight = false;
	}
}

// ---- Slot list ----

void USaveSubsystem::UpdateSlotInfo(const FSaveSlotHeader& Header)
{
	EnsureSlotInfos();
	if (SlotInfos.IsValidIndex(Header.SlotIndex))
	{
		FSaveSlotInfo& Info = SlotInfos[Header.SlotIndex];
		Info.Header = Header;
		Info.bOccupied = true;
		Info.bRestoredFromBackup = false;
	}
	OnSlotsChanged.Broadcast();
}

FSaveSlotInfo USaveSubsystem::ReadSlotInfo(const int32 SlotIndex) const
{
	FSaveSlotInfo Info;
	USaveFrameworkSaveGame* SaveGame = nullptr;
	const ESaveSlotLoadResult Result = FSaveSlotStorage::LoadWithFallback(GetSlotName(SlotIndex), SaveGame);
	if (SaveGame)
	{
		Info.Header = SaveGame->Header;
		Info.bOccupied = true;
		Info.bRestoredFromBackup = Result == ESaveSlotLoadResult::RestoredFromBackup;
	}
	Info.Header.SlotIndex = SlotIndex;
	return Info;
}

void USaveSubsystem::EnsureSlotInfos() const
{
	if (bSlotInfosRead)
	{
		return;
	}
	const int32 NumSlots = USaveFrameworkSettings::Get().NumSlots;
	SlotInfos.Reset(NumSlots);
	for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
	{
		SlotInfos.Add(ReadSlotInfo(SlotIndex));
	}
	bSlotInfosRead = true;
}

#undef LOCTEXT_NAMESPACE
