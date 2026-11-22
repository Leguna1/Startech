#include "Runtime/QuestSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Bounties/BountyFactory.h"
#include "Bounties/BountyTemplate.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Objectives/QuestObjective.h"
#include "QuestFrameworkLog.h"
#include "QuestFrameworkSettings.h"

#define LOCTEXT_NAMESPACE "QuestSubsystem"

void UQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadDefinitionsFromSettings();
	bInitialized = true;

	UE_LOG(LogQuestFramework, Log, TEXT("Quest subsystem ready with %d quest(s)."), Definitions.Num());
}

void UQuestSubsystem::Deinitialize()
{
	bInitialized = false;
	Definitions.Reset();
	States.Reset();
	PendingNotifications.Reset();
	InventoryProvider.Reset();
	RewardGranter.Reset();
	TurnInGate.Unbind();

	Super::Deinitialize();
}

void UQuestSubsystem::LoadDefinitionsFromSettings()
{
	const UQuestFrameworkSettings& Settings = UQuestFrameworkSettings::Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UQuestDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	for (const FDirectoryPath& Directory : Settings.QuestDefinitionPaths)
	{
		if (!Directory.Path.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*Directory.Path));
		}
	}
	if (Filter.PackagePaths.IsEmpty())
	{
		return;
	}

	IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	if (!FPlatformProperties::RequiresCookedData() || Registry.IsLoadingAssets())
	{
		// Uncooked (editor, PIE, -game from the editor binary) the folders may not be scanned yet, or not at all in
		// -game. Quest folders are small. A cooked build reads the registry the cook wrote.
		TArray<FString> Paths;
		for (const FName& Path : Filter.PackagePaths)
		{
			Paths.Add(Path.ToString());
		}
		Registry.ScanPathsSynchronous(Paths);
	}

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	// Sorted so the registry order (and GetQuestsByStatus) does not depend on scan order.
	Assets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.PackageName.LexicalLess(B.PackageName);
	});

	for (const FAssetData& Asset : Assets)
	{
		if (UQuestDefinition* const Definition = Cast<UQuestDefinition>(Asset.GetAsset()))
		{
			RegisterDefinition(Definition);
		}
	}

	// Prerequisites can point at quests registered after them, so statuses are settled once all are in.
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		States[Index] = MakeInitialState(*Definitions[Index]);
	}
	PendingNotifications.Reset();
}

bool UQuestSubsystem::RegisterDefinition(UQuestDefinition* const Definition)
{
	if (!Definition || Definition->QuestId.IsNone())
	{
		UE_LOG(LogQuestFramework, Error, TEXT("RegisterDefinition: %s has no QuestId."), *GetNameSafe(Definition));
		return false;
	}
	if (FindIndex(Definition->QuestId) != INDEX_NONE)
	{
		UE_LOG(LogQuestFramework, Error, TEXT("RegisterDefinition: quest id '%s' (%s) is already registered."),
			*Definition->QuestId.ToString(), *Definition->GetPathName());
		return false;
	}

	Definitions.Add(Definition);
	States.Add(MakeInitialState(*Definition));
	return true;
}

bool UQuestSubsystem::UnregisterDefinition(const FName QuestId)
{
	const int32 Index = FindIndex(QuestId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Definitions.RemoveAt(Index);
	States.RemoveAt(Index);

	FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
	Note.Kind = FPendingNotification::EKind::Unregistered;
	Note.QuestId = QuestId;
	FlushNotifications();
	return true;
}

void UQuestSubsystem::SetInventoryProvider(IQuestInventoryProvider* const Provider)
{
	InventoryProvider = TWeakInterfacePtr<IQuestInventoryProvider>(Provider);
	RefreshInventoryObjectives();
}

void UQuestSubsystem::SetRewardGranter(IQuestRewardGranter* const Granter)
{
	RewardGranter = TWeakInterfacePtr<IQuestRewardGranter>(Granter);
}

void UQuestSubsystem::SetTurnInGate(const FQuestTurnInGate& Gate)
{
	TurnInGate = Gate;
}

bool UQuestSubsystem::CanAccept(const FName QuestId, FText& OutReason) const
{
	const int32 Index = FindIndex(QuestId);
	if (Index == INDEX_NONE)
	{
		OutReason = FText::Format(LOCTEXT("Unknown", "Unknown quest '{0}'."), FText::FromName(QuestId));
		return false;
	}

	const UQuestDefinition& Definition = *Definitions[Index];
	const FQuestRuntimeState& State = States[Index];

	switch (State.Status)
	{
	case EQuestStatus::Locked:
		OutReason = LOCTEXT("Locked", "Its prerequisites have not been completed.");
		return false;
	case EQuestStatus::Active:
	case EQuestStatus::ReadyToTurnIn:
		OutReason = LOCTEXT("AlreadyActive", "It is already active.");
		return false;
	case EQuestStatus::Completed:
		if (!Definition.bRepeatable)
		{
			OutReason = LOCTEXT("AlreadyCompleted", "It has already been completed.");
			return false;
		}
		break;
	default:
		break;
	}

	if (State.CooldownRemainingSeconds > 0.f)
	{
		OutReason = FText::Format(LOCTEXT("Cooldown", "Available again in {0} s."),
			FText::AsNumber(FMath::CeilToInt(State.CooldownRemainingSeconds)));
		return false;
	}

	const int32 Limit = UQuestFrameworkSettings::Get().GetCategoryLimit(Definition.Category);
	if (Limit != INDEX_NONE && CountActiveInCategory(Definition.Category) >= Limit)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Max"), FText::AsNumber(Limit));
		Args.Add(TEXT("DisplayName"), GetCategoryText(Definition));
		OutReason = FText::Format(LOCTEXT("CategoryFull", "Limit reached: {Max} {DisplayName}"), Args);
		return false;
	}

	return true;
}

bool UQuestSubsystem::Accept(const FName QuestId, FText& OutReason)
{
	if (!CanAccept(QuestId, OutReason))
	{
		return false;
	}

	const int32 Index = FindIndex(QuestId);
	const UQuestDefinition& Definition = *Definitions[Index];
	FQuestRuntimeState& State = States[Index];

	SetStatus(State, EQuestStatus::Active);
	State.ObjectiveProgress.Init(0, Definition.Objectives.Num());
	for (int32 Objective = 0; Objective < Definition.Objectives.Num(); ++Objective)
	{
		if (const UQuestObjective* const Each = Definition.Objectives[Objective])
		{
			FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
			Note.Kind = FPendingNotification::EKind::Progress;
			Note.QuestId = QuestId;
			Note.ObjectiveIndex = Objective;
			Note.Required = Each->RequiredCount;
		}
	}

	State.TimeRemainingSeconds = Definition.HasTimeLimit() ? Definition.TimeLimitSeconds : 0.f;
	if (Definition.HasTimeLimit())
	{
		FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
		Note.Kind = FPendingNotification::EKind::Time;
		Note.QuestId = QuestId;
		Note.Seconds = State.TimeRemainingSeconds;
	}

	RefreshInventoryProgress(Definition, State);
	const bool bAutoComplete = UpdateReadiness(Definition, State);

	UE_LOG(LogQuestFramework, Log, TEXT("Accepted quest '%s'."), *QuestId.ToString());
	if (bAutoComplete)
	{
		FText Ignored;
		CompleteQuest(QuestId, Ignored);
	}

	FlushNotifications();
	return true;
}

bool UQuestSubsystem::Abandon(const FName QuestId)
{
	const int32 Index = FindIndex(QuestId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	FQuestRuntimeState& State = States[Index];
	if (State.Status != EQuestStatus::Active && State.Status != EQuestStatus::ReadyToTurnIn)
	{
		return false;
	}

	State.TimeRemainingSeconds = 0.f;
	State.ObjectiveProgress.Reset();
	SetStatus(State, EQuestStatus::Available);
	UE_LOG(LogQuestFramework, Log, TEXT("Abandoned quest '%s'."), *QuestId.ToString());

	// A generated bounty does not go back on the board.
	const bool bGenerated = Definitions[Index]->IsGenerated();
	FlushNotifications();
	if (bGenerated)
	{
		UnregisterDefinition(QuestId);
	}
	return true;
}

bool UQuestSubsystem::CanTurnIn(const FName QuestId, FText& OutReason) const
{
	const int32 Index = FindIndex(QuestId);
	if (Index == INDEX_NONE)
	{
		OutReason = FText::Format(LOCTEXT("Unknown", "Unknown quest '{0}'."), FText::FromName(QuestId));
		return false;
	}

	const UQuestDefinition& Definition = *Definitions[Index];
	const FQuestRuntimeState& State = States[Index];

	if (State.Status != EQuestStatus::ReadyToTurnIn)
	{
		OutReason = LOCTEXT("NotReady", "It is not ready to turn in.");
		return false;
	}
	if (TurnInGate.IsBound() && !TurnInGate.Execute(QuestId, OutReason))
	{
		return false;
	}

	// The inventory may have changed since the quest became ready.
	if (!WouldObjectivesBeComplete(Definition, State))
	{
		OutReason = LOCTEXT("NoLongerComplete", "Its objectives are no longer complete.");
		return false;
	}

	if (!InventoryProvider.IsValid())
	{
		for (const UQuestObjective* const Objective : Definition.Objectives)
		{
			if (Objective && Objective->NeedsInventoryOnTurnIn())
			{
				OutReason = LOCTEXT("NoInventory", "No inventory is registered to take the items from.");
				return false;
			}
		}
	}

	return true;
}

bool UQuestSubsystem::TurnIn(const FName QuestId, FText& OutReason)
{
	if (!CanTurnIn(QuestId, OutReason))
	{
		// A quest that is no longer complete goes back to Active, as it would on the next inventory refresh.
		const int32 Index = FindIndex(QuestId);
		if (Index != INDEX_NONE && States[Index].Status == EQuestStatus::ReadyToTurnIn)
		{
			FQuestRuntimeState& State = States[Index];
			RefreshInventoryProgress(*Definitions[Index], State);
			UpdateReadiness(*Definitions[Index], State);
			FlushNotifications();
		}
		return false;
	}

	const bool bCompleted = CompleteQuest(QuestId, OutReason);
	FlushNotifications();
	return bCompleted;
}

void UQuestSubsystem::ReportEvent(const FQuestEvent& Event)
{
	if (!Event.EventTag.IsValid())
	{
		return;
	}

	TArray<FName> ToComplete;
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		FQuestRuntimeState& State = States[Index];
		if (State.Status != EQuestStatus::Active && State.Status != EQuestStatus::ReadyToTurnIn)
		{
			continue;
		}

		const UQuestDefinition& Definition = *Definitions[Index];
		for (int32 Objective = 0; Objective < Definition.Objectives.Num(); ++Objective)
		{
			const UQuestObjective* const Each = Definition.Objectives[Objective];
			if (!Each || Each->TracksInventory() || !IsObjectiveCounting(Definition, State, Objective))
			{
				continue;
			}

			const int32 Added = Each->EvaluateEvent(Event);
			if (Added > 0)
			{
				SetProgress(Definition, State, Objective, State.ObjectiveProgress[Objective] + Added);
			}
		}

		// Item events, and any event that moved an in-order quest onto an inventory objective, re-read the inventory.
		RefreshInventoryProgress(Definition, State);

		if (UpdateReadiness(Definition, State))
		{
			ToComplete.Add(Definition.QuestId);
		}
	}

	// After the loop: completing can remove a generated quest from the arrays.
	for (const FName& QuestId : ToComplete)
	{
		FText Ignored;
		CompleteQuest(QuestId, Ignored);
	}
	FlushNotifications();
}

void UQuestSubsystem::RefreshInventoryObjectives()
{
	TArray<FName> ToComplete;
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		FQuestRuntimeState& State = States[Index];
		if (State.Status == EQuestStatus::Active || State.Status == EQuestStatus::ReadyToTurnIn)
		{
			RefreshInventoryProgress(*Definitions[Index], State);
			if (UpdateReadiness(*Definitions[Index], State))
			{
				ToComplete.Add(Definitions[Index]->QuestId);
			}
		}
	}

	for (const FName& QuestId : ToComplete)
	{
		FText Ignored;
		CompleteQuest(QuestId, Ignored);
	}
	FlushNotifications();
}

bool UQuestSubsystem::GetState(const FName QuestId, FQuestRuntimeState& OutState) const
{
	const FQuestRuntimeState* const State = FindState(QuestId);
	if (State)
	{
		OutState = *State;
	}
	return State != nullptr;
}

const FQuestRuntimeState* UQuestSubsystem::FindState(const FName QuestId) const
{
	const int32 Index = FindIndex(QuestId);
	return Index != INDEX_NONE ? &States[Index] : nullptr;
}

TArray<FName> UQuestSubsystem::GetQuestsByStatus(const EQuestStatus Status) const
{
	TArray<FName> Ids;
	for (const FQuestRuntimeState& State : States)
	{
		if (State.Status == Status)
		{
			Ids.Add(State.QuestId);
		}
	}
	return Ids;
}

UQuestDefinition* UQuestSubsystem::GetDefinition(const FName QuestId) const
{
	const int32 Index = FindIndex(QuestId);
	return Index != INDEX_NONE ? Definitions[Index].Get() : nullptr;
}

int32 UQuestSubsystem::CountActiveInCategory(const FGameplayTag Category) const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		const EQuestStatus Status = States[Index].Status;
		if ((Status == EQuestStatus::Active || Status == EQuestStatus::ReadyToTurnIn) && Definitions[Index]->Category == Category)
		{
			++Count;
		}
	}
	return Count;
}

TArray<FQuestRuntimeState> UQuestSubsystem::GetAllStates() const
{
	return States;
}

void UQuestSubsystem::RestoreStates(const TArray<FQuestRuntimeState>& SavedStates)
{
	// Generated bounties are rebuilt from the save, not kept from this session.
	for (int32 Index = Definitions.Num() - 1; Index >= 0; --Index)
	{
		if (Definitions[Index]->IsGenerated())
		{
			Definitions.RemoveAt(Index);
			States.RemoveAt(Index);
		}
	}

	const UQuestFrameworkSettings& Settings = UQuestFrameworkSettings::Get();
	for (const FQuestRuntimeState& Saved : SavedStates)
	{
		if (Saved.SourceTemplateId.IsNone())
		{
			continue;
		}

		const UBountyTemplate* const Template = Settings.FindBountyTemplate(Saved.SourceTemplateId);
		UQuestDefinition* const Definition = Template ? FBountyFactory::Generate(*Template, Saved.GenerationSeed, this) : nullptr;
		if (!Definition || Definition->QuestId != Saved.QuestId)
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("RestoreStates: bounty '%s' could not be regenerated from template '%s' (seed %d); skipped."),
				*Saved.QuestId.ToString(), *Saved.SourceTemplateId.ToString(), Saved.GenerationSeed);
			continue;
		}
		RegisterDefinition(Definition);
	}

	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		States[Index] = MakeInitialState(*Definitions[Index]);
	}

	for (const FQuestRuntimeState& Saved : SavedStates)
	{
		const int32 Index = FindIndex(Saved.QuestId);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("RestoreStates: no quest '%s' any more; its saved state is skipped."),
				*Saved.QuestId.ToString());
			continue;
		}

		FQuestRuntimeState& State = States[Index];
		State = Saved;

		// The definition may have gained or lost objectives since the save.
		const UQuestDefinition& Definition = *Definitions[Index];
		const bool bAccepted = State.Status == EQuestStatus::Active || State.Status == EQuestStatus::ReadyToTurnIn;
		State.ObjectiveProgress.SetNum(bAccepted ? Definition.Objectives.Num() : 0);
		for (int32 Objective = 0; Objective < State.ObjectiveProgress.Num(); ++Objective)
		{
			const UQuestObjective* const Each = Definition.Objectives[Objective];
			State.ObjectiveProgress[Objective] = FMath::Clamp(State.ObjectiveProgress[Objective], 0, Each ? Each->RequiredCount : 0);
		}
	}

	PendingNotifications.Reset();
	UnlockQuests();
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		FQuestRuntimeState& State = States[Index];
		if (State.Status == EQuestStatus::Active || State.Status == EQuestStatus::ReadyToTurnIn)
		{
			RefreshInventoryProgress(*Definitions[Index], State);
			UpdateReadiness(*Definitions[Index], State);
		}
	}

	// One status event per quest, so listeners rebuild from scratch.
	PendingNotifications.Reset();
	for (const FQuestRuntimeState& State : States)
	{
		FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
		Note.Kind = FPendingNotification::EKind::Status;
		Note.QuestId = State.QuestId;
		Note.Status = State.Status;
	}

	UE_LOG(LogQuestFramework, Log, TEXT("Restored %d saved quest state(s)."), SavedStates.Num());
	FlushNotifications();
}

void UQuestSubsystem::Tick(const float DeltaTime)
{
	// DeltaTime is the world's (dilated) frame time, and this is not called while the world is paused.
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		const UQuestDefinition& Definition = *Definitions[Index];
		FQuestRuntimeState& State = States[Index];

		if (State.Status == EQuestStatus::Active && Definition.HasTimeLimit())
		{
			const float Before = State.TimeRemainingSeconds;
			State.TimeRemainingSeconds = FMath::Max(Before - DeltaTime, 0.f);

			// Once per whole second, not per frame.
			if (FMath::CeilToInt(State.TimeRemainingSeconds) != FMath::CeilToInt(Before))
			{
				FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
				Note.Kind = FPendingNotification::EKind::Time;
				Note.QuestId = State.QuestId;
				Note.Seconds = State.TimeRemainingSeconds;
			}

			if (State.TimeRemainingSeconds <= 0.f)
			{
				FailQuest(Definition, State);
			}
		}
		else if (State.CooldownRemainingSeconds > 0.f)
		{
			State.CooldownRemainingSeconds = FMath::Max(State.CooldownRemainingSeconds - DeltaTime, 0.f);
			if (State.CooldownRemainingSeconds <= 0.f
				&& (State.Status == EQuestStatus::Completed || State.Status == EQuestStatus::Failed))
			{
				SetStatus(State, ArePrerequisitesMet(Definition) ? EQuestStatus::Available : EQuestStatus::Locked);
			}
		}
	}
	FlushNotifications();
}

TStatId UQuestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UQuestSubsystem, STATGROUP_Tickables);
}

ETickableTickType UQuestSubsystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UQuestSubsystem::IsTickable() const
{
	if (!bInitialized)
	{
		return false;
	}
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		const FQuestRuntimeState& State = States[Index];
		if (State.CooldownRemainingSeconds > 0.f || (State.Status == EQuestStatus::Active && Definitions[Index]->HasTimeLimit()))
		{
			return true;
		}
	}
	return false;
}

UWorld* UQuestSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

int32 UQuestSubsystem::FindIndex(const FName QuestId) const
{
	return States.IndexOfByPredicate([QuestId](const FQuestRuntimeState& State)
	{
		return State.QuestId == QuestId;
	});
}

bool UQuestSubsystem::ArePrerequisitesMet(const UQuestDefinition& Definition) const
{
	for (const FName& Prerequisite : Definition.PrerequisiteQuestIds)
	{
		const FQuestRuntimeState* const State = FindState(Prerequisite);
		if (!State || State->TimesCompleted <= 0)
		{
			return false;
		}
	}
	return true;
}

FQuestRuntimeState UQuestSubsystem::MakeInitialState(const UQuestDefinition& Definition) const
{
	FQuestRuntimeState State;
	State.QuestId = Definition.QuestId;
	State.Status = ArePrerequisitesMet(Definition) ? EQuestStatus::Available : EQuestStatus::Locked;
	State.SourceTemplateId = Definition.SourceTemplateId;
	State.GenerationSeed = Definition.GenerationSeed;
	return State;
}

void UQuestSubsystem::SetStatus(FQuestRuntimeState& State, const EQuestStatus Status)
{
	if (State.Status == Status)
	{
		return;
	}

	State.Status = Status;
	FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
	Note.Kind = FPendingNotification::EKind::Status;
	Note.QuestId = State.QuestId;
	Note.Status = Status;
}

void UQuestSubsystem::SetProgress(const UQuestDefinition& Definition, FQuestRuntimeState& State, const int32 ObjectiveIndex,
	const int32 Value)
{
	const UQuestObjective* const Objective = Definition.Objectives.IsValidIndex(ObjectiveIndex) ? Definition.Objectives[ObjectiveIndex].Get() : nullptr;
	if (!Objective || !State.ObjectiveProgress.IsValidIndex(ObjectiveIndex))
	{
		return;
	}

	const int32 Clamped = FMath::Clamp(Value, 0, Objective->RequiredCount);
	if (State.ObjectiveProgress[ObjectiveIndex] == Clamped)
	{
		return;
	}

	State.ObjectiveProgress[ObjectiveIndex] = Clamped;
	FPendingNotification& Note = PendingNotifications.AddDefaulted_GetRef();
	Note.Kind = FPendingNotification::EKind::Progress;
	Note.QuestId = State.QuestId;
	Note.ObjectiveIndex = ObjectiveIndex;
	Note.Current = Clamped;
	Note.Required = Objective->RequiredCount;
}

bool UQuestSubsystem::IsObjectiveComplete(const UQuestDefinition& Definition, const FQuestRuntimeState& State,
	const int32 ObjectiveIndex)
{
	const UQuestObjective* const Objective = Definition.Objectives.IsValidIndex(ObjectiveIndex) ? Definition.Objectives[ObjectiveIndex].Get() : nullptr;
	return Objective && State.ObjectiveProgress.IsValidIndex(ObjectiveIndex)
		&& State.ObjectiveProgress[ObjectiveIndex] >= Objective->RequiredCount;
}

bool UQuestSubsystem::IsObjectiveCounting(const UQuestDefinition& Definition, const FQuestRuntimeState& State,
	const int32 ObjectiveIndex)
{
	if (!Definition.bObjectivesInOrder)
	{
		return true;
	}
	for (int32 Earlier = 0; Earlier < ObjectiveIndex; ++Earlier)
	{
		if (!IsObjectiveComplete(Definition, State, Earlier))
		{
			return false;
		}
	}
	return true;
}

bool UQuestSubsystem::AreObjectivesComplete(const UQuestDefinition& Definition, const FQuestRuntimeState& State)
{
	for (int32 Objective = 0; Objective < Definition.Objectives.Num(); ++Objective)
	{
		if (!IsObjectiveComplete(Definition, State, Objective))
		{
			return false;
		}
	}
	return !Definition.Objectives.IsEmpty();
}

void UQuestSubsystem::RefreshInventoryProgress(const UQuestDefinition& Definition, FQuestRuntimeState& State)
{
	const IQuestInventoryProvider* const Inventory = InventoryProvider.Get();

	// In order, so an earlier inventory objective that drops below its count stops the later ones counting.
	for (int32 Objective = 0; Objective < Definition.Objectives.Num(); ++Objective)
	{
		const UQuestObjective* const Each = Definition.Objectives[Objective];
		if (!Each || !Each->TracksInventory())
		{
			continue;
		}

		const int32 Current = Inventory && IsObjectiveCounting(Definition, State, Objective) ? Each->QueryCurrent(*Inventory) : 0;
		SetProgress(Definition, State, Objective, Current);
	}
}

bool UQuestSubsystem::WouldObjectivesBeComplete(const UQuestDefinition& Definition, const FQuestRuntimeState& State) const
{
	const IQuestInventoryProvider* const Inventory = InventoryProvider.Get();

	// The same reads as RefreshInventoryProgress, on a copy, so nothing is changed or notified.
	FQuestRuntimeState Preview = State;
	for (int32 Objective = 0; Objective < Definition.Objectives.Num(); ++Objective)
	{
		const UQuestObjective* const Each = Definition.Objectives[Objective];
		if (!Each || !Each->TracksInventory() || !Preview.ObjectiveProgress.IsValidIndex(Objective))
		{
			continue;
		}

		const int32 Current = Inventory && IsObjectiveCounting(Definition, Preview, Objective) ? Each->QueryCurrent(*Inventory) : 0;
		Preview.ObjectiveProgress[Objective] = FMath::Clamp(Current, 0, Each->RequiredCount);
	}
	return AreObjectivesComplete(Definition, Preview);
}

bool UQuestSubsystem::UpdateReadiness(const UQuestDefinition& Definition, FQuestRuntimeState& State)
{
	const bool bComplete = AreObjectivesComplete(Definition, State);
	if (!bComplete)
	{
		// Lost items can take a ready quest back to active.
		SetStatus(State, EQuestStatus::Active);
		return false;
	}
	if (!Definition.bRequiresTurnIn)
	{
		return true;
	}
	SetStatus(State, EQuestStatus::ReadyToTurnIn);
	return false;
}

bool UQuestSubsystem::CompleteQuest(const FName QuestId, FText& OutReason)
{
	const int32 Index = FindIndex(QuestId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	UQuestDefinition* const Definition = Definitions[Index];
	FQuestRuntimeState& State = States[Index];

	// The inventory may have changed since the quest became ready.
	RefreshInventoryProgress(*Definition, State);
	if (!AreObjectivesComplete(*Definition, State))
	{
		SetStatus(State, EQuestStatus::Active);
		OutReason = LOCTEXT("NoLongerComplete", "Its objectives are no longer complete.");
		return false;
	}

	IQuestInventoryProvider* const Inventory = InventoryProvider.Get();
	for (const UQuestObjective* const Objective : Definition->Objectives)
	{
		if (Objective && Objective->NeedsInventoryOnTurnIn() && !Inventory)
		{
			OutReason = LOCTEXT("NoInventory", "No inventory is registered to take the items from.");
			return false;
		}
	}
	for (const UQuestObjective* const Objective : Definition->Objectives)
	{
		if (Objective && Objective->NeedsInventoryOnTurnIn() && !Objective->ApplyTurnIn(*Inventory))
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Quest '%s': the inventory refused to remove items for an objective."),
				*QuestId.ToString());
		}
	}

	IQuestRewardGranter* const Granter = RewardGranter.Get();
	for (const FQuestReward& Reward : Definition->Rewards)
	{
		if (Granter)
		{
			Granter->GrantReward(Reward);
		}
		else
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Quest '%s': no reward granter registered; %d x %s not granted."),
				*QuestId.ToString(), Reward.Amount, *Reward.RewardTag.ToString());
		}
	}

	++State.TimesCompleted;
	State.TimeRemainingSeconds = 0.f;
	State.ObjectiveProgress.Reset();
	SetStatus(State, EQuestStatus::Completed);
	if (Definition->bRepeatable)
	{
		State.CooldownRemainingSeconds = FMath::Max(Definition->RepeatCooldownSeconds, 0.f);
		if (State.CooldownRemainingSeconds <= 0.f)
		{
			SetStatus(State, EQuestStatus::Available);
		}
	}
	UE_LOG(LogQuestFramework, Log, TEXT("Completed quest '%s' (%d time(s))."), *QuestId.ToString(), State.TimesCompleted);

	UnlockQuests();

	// A generated bounty is done with once turned in.
	if (Definition->IsGenerated())
	{
		FlushNotifications();
		UnregisterDefinition(QuestId);
	}
	return true;
}

void UQuestSubsystem::FailQuest(const UQuestDefinition& Definition, FQuestRuntimeState& State)
{
	UE_LOG(LogQuestFramework, Log, TEXT("Quest '%s' failed: time ran out."), *State.QuestId.ToString());

	State.TimeRemainingSeconds = 0.f;
	State.ObjectiveProgress.Reset();
	SetStatus(State, EQuestStatus::Failed);

	if (Definition.bRepeatable)
	{
		State.CooldownRemainingSeconds = FMath::Max(Definition.RepeatCooldownSeconds, 0.f);
		if (State.CooldownRemainingSeconds > 0.f)
		{
			return;
		}
	}
	SetStatus(State, EQuestStatus::Available);
}

void UQuestSubsystem::UnlockQuests()
{
	for (int32 Index = 0; Index < States.Num(); ++Index)
	{
		if (States[Index].Status == EQuestStatus::Locked && ArePrerequisitesMet(*Definitions[Index]))
		{
			SetStatus(States[Index], EQuestStatus::Available);
		}
	}
}

void UQuestSubsystem::FlushNotifications()
{
	if (PendingNotifications.IsEmpty())
	{
		return;
	}

	// Taken out first: a listener may call back in and queue (and flush) its own.
	const TArray<FPendingNotification> Notes = MoveTemp(PendingNotifications);
	PendingNotifications.Reset();

	for (const FPendingNotification& Note : Notes)
	{
		switch (Note.Kind)
		{
		case FPendingNotification::EKind::Status:
			OnQuestStatusChanged.Broadcast(Note.QuestId, Note.Status);
			break;
		case FPendingNotification::EKind::Progress:
			OnObjectiveProgress.Broadcast(Note.QuestId, Note.ObjectiveIndex, Note.Current, Note.Required);
			break;
		case FPendingNotification::EKind::Time:
			OnQuestTimeChanged.Broadcast(Note.QuestId, Note.Seconds);
			break;
		case FPendingNotification::EKind::Unregistered:
			OnQuestUnregistered.Broadcast(Note.QuestId);
			break;
		}
	}
}

FText UQuestSubsystem::GetCategoryText(const UQuestDefinition& Definition)
{
	const FQuestCategoryLimit* const Limit = UQuestFrameworkSettings::Get().FindCategoryLimit(Definition.Category);
	if (Limit && !Limit->DisplayName.IsEmpty())
	{
		return Limit->DisplayName;
	}

	// The last segment of the tag: "Bounty" for Quest.Category.Bounty.
	FString Name = Definition.Category.ToString();
	int32 Dot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), Dot))
	{
		Name.RightChopInline(Dot + 1);
	}
	return FText::FromString(Name);
}

#undef LOCTEXT_NAMESPACE
