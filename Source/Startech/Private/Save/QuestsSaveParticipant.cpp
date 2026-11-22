#include "Save/QuestsSaveParticipant.h"

#include "Bounties/BountyBoardSubsystem.h"
#include "Runtime/QuestSubsystem.h"
#include "Save/StartechSaveLog.h"
#include "SaveSerialization.h"

const FName UQuestsSaveParticipant::ParticipantId(TEXT("Startech.Quests"));

void UQuestsSaveParticipant::Init(UQuestSubsystem& InQuests, UBountyBoardSubsystem& InBoard)
{
	Quests = &InQuests;
	Board = &InBoard;

	InQuests.OnQuestStatusChanged.AddUniqueDynamic(this, &UQuestsSaveParticipant::HandleQuestStatusChanged);
	InQuests.OnObjectiveProgress.AddUniqueDynamic(this, &UQuestsSaveParticipant::HandleObjectiveProgress);
	InQuests.OnQuestTimeChanged.AddUniqueDynamic(this, &UQuestsSaveParticipant::HandleQuestTimeChanged);
	InBoard.OnBoardRefreshed.AddUniqueDynamic(this, &UQuestsSaveParticipant::HandleBoardRefreshed);
	RefreshLastSave();
}

void UQuestsSaveParticipant::RefreshLastSave() const
{
	const UQuestSubsystem* const QuestSubsystem = Quests.Get();
	const UBountyBoardSubsystem* const BountyBoard = Board.Get();

	// A running quest subsystem always holds definitions; none means it has been shut down.
	if (QuestSubsystem && BountyBoard && !QuestSubsystem->GetAllDefinitions().IsEmpty())
	{
		LastSave.States = QuestSubsystem->GetAllStates();
		LastSave.NextBountySeed = BountyBoard->GetNextSeed();
	}
}

void UQuestsSaveParticipant::HandleQuestStatusChanged(const FName QuestId, const EQuestStatus NewStatus)
{
	RefreshLastSave();
}

void UQuestsSaveParticipant::HandleObjectiveProgress(const FName QuestId, const int32 ObjectiveIndex, const int32 Current,
	const int32 Required)
{
	RefreshLastSave();
}

void UQuestsSaveParticipant::HandleQuestTimeChanged(const FName QuestId, const float SecondsRemaining)
{
	RefreshLastSave();
}

void UQuestsSaveParticipant::HandleBoardRefreshed()
{
	RefreshLastSave();
}

FName UQuestsSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 UQuestsSaveParticipant::GetSaveVersion() const
{
	// 2: sector tiers changed the bounty template's locations. 3: Terra's bounties moved to DA_BountyTemplate_Terra and
	// DA_BountyTemplate_Standard lost Terra. Bounties from older versions are dropped (see ReadSaveData).
	return 3;
}

void UQuestsSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	const UQuestSubsystem* const QuestSubsystem = Quests.Get();
	if (!QuestSubsystem || QuestSubsystem->GetAllDefinitions().IsEmpty())
	{
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Quests: the quest subsystem is shut down; writing the quests as last seen."));
	}
	RefreshLastSave();
	FSaveSerialization::StructToBytes(LastSave, OutData);
}

void UQuestsSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechQuestsSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Quests: unreadable record; quests start fresh instead."));
		OnNewGame();
		return;
	}
	// Bounties before version 3 came from older templates (version 1: before the tier locations; version 2: before the
	// Terra template). Their seeds would regenerate into other bounties, so active and offered ones are dropped and the
	// board is refilled once the player is placed.
	bBountiesDropped = false;
	if (SavedVersion < 3)
	{
		const int32 Removed = Save.States.RemoveAll([](const FQuestRuntimeState& State)
		{
			return !State.SourceTemplateId.IsNone();
		});
		bBountiesDropped = true;
		UE_LOG(LogStartechSave, Log, TEXT("Startech.Quests: version %d save; %d generated bounty state(s) dropped."),
			SavedVersion, Removed);
	}

	Apply(Save);
	bStartedFresh = false;
	UE_LOG(LogStartechSave, Log, TEXT("Startech.Quests: %d quest state(s), next bounty seed %d."), Save.States.Num(),
		Save.NextBountySeed);
}

void UQuestsSaveParticipant::OnNewGame()
{
	FStartechQuestsSave Fresh;
	// Positive and well away from overflow, like the board's own start value.
	Fresh.NextBountySeed = FMath::RandRange(1, 1 << 24);
	Apply(Fresh);
	bStartedFresh = true;
	bBountiesDropped = false;
}

void UQuestsSaveParticipant::Apply(const FStartechQuestsSave& Save)
{
	UQuestSubsystem* const QuestSubsystem = Quests.Get();
	UBountyBoardSubsystem* const BountyBoard = Board.Get();
	if (!QuestSubsystem || !BountyBoard)
	{
		return;
	}

	TGuardValue<bool> RestoringGuard(bRestoring, true);
	QuestSubsystem->RestoreStates(Save.States);
	BountyBoard->SetNextSeed(Save.NextBountySeed);

	LastSave.States = QuestSubsystem->GetAllStates();
	LastSave.NextBountySeed = Save.NextBountySeed;
}
