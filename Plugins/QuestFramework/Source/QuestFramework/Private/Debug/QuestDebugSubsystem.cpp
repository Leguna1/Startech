#include "Debug/QuestDebugSubsystem.h"

#include "Definitions/QuestDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Objectives/QuestObjective.h"
#include "Runtime/QuestSubsystem.h"

static TAutoConsoleVariable<int32> CVarQuestDebug(
	TEXT("Quest.Debug"),
	0,
	TEXT("1 shows the active quests with objective progress and time left. 0 hides them."),
	ECVF_Default);

void UQuestDebugSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* const World = GetWorld();
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	const UQuestSubsystem* const Quests = GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr;
	if (!Quests || CVarQuestDebug.GetValueOnGameThread() <= 0)
	{
		return;
	}

	int32 Line = 0;
	int32 Shown = 0;
	for (const UQuestDefinition* const Definition : Quests->GetAllDefinitions())
	{
		const FQuestRuntimeState* const State = Quests->FindState(Definition->QuestId);
		if (!State || (State->Status != EQuestStatus::Active && State->Status != EQuestStatus::ReadyToTurnIn))
		{
			continue;
		}

		++Shown;
		FString Header = FString::Printf(TEXT("%s  [%s]"), *Definition->Title.ToString(), *Definition->QuestId.ToString());
		if (State->Status == EQuestStatus::ReadyToTurnIn)
		{
			Header += TEXT("  READY TO TURN IN");
		}
		else if (Definition->HasTimeLimit())
		{
			const int32 Seconds = FMath::CeilToInt(State->TimeRemainingSeconds);
			Header += FString::Printf(TEXT("  %d:%02d left"), Seconds / 60, Seconds % 60);
		}
		PrintLine(Line++, Header, State->Status == EQuestStatus::ReadyToTurnIn ? FColor::Green : FColor::Cyan);

		for (int32 Index = 0; Index < Definition->Objectives.Num() && Index < State->ObjectiveProgress.Num(); ++Index)
		{
			if (const UQuestObjective* const Objective = Definition->Objectives[Index])
			{
				const bool bDone = State->ObjectiveProgress[Index] >= Objective->RequiredCount;
				PrintLine(Line++, FString::Printf(TEXT("    %s %s"), bDone ? TEXT("[x]") : TEXT("[ ]"),
					*Objective->GetProgressText(State->ObjectiveProgress[Index]).ToString()), bDone ? FColor::Silver : FColor::White);
			}
		}
	}

	if (Shown == 0)
	{
		PrintLine(Line++, TEXT("No active quests (Quest.List shows them all)."), FColor::Silver);
	}
}

TStatId UQuestDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UQuestDebugSubsystem, STATGROUP_Tickables);
}

bool UQuestDebugSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
#endif
}

void UQuestDebugSubsystem::PrintLine(const int32 LineIndex, const FString& Text, const FColor& Color)
{
	if (GEngine)
	{
		// Zero duration shows the line for exactly one frame; it is printed again every tick.
		GEngine->AddOnScreenDebugMessage(FirstMessageKey + LineIndex, 0.f, Color, Text);
	}
}
