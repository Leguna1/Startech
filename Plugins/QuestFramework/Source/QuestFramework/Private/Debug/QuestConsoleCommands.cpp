#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Bounties/BountyBoardSubsystem.h"
#include "Debug/QuestDebugBridge.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Objectives/QuestObjective.h"
#include "QuestFrameworkLog.h"
#include "QuestTags.h"
#include "Runtime/QuestSubsystem.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Console commands (Command pattern: each command is one small function bound to one name). Non-shipping builds only.
 *   Quest.List                                               every quest with status, progress and time left
 *   Quest.Accept <Id> / Quest.Abandon <Id> / Quest.TurnIn <Id>
 *   Quest.Event <EventTag> <SubjectTag|None> [ContextId] [Amount]
 *   Quest.Bounty.Refresh / Quest.Bounty.List
 *   Quest.Debug.UseTestBridge                                registers a stand-in inventory and reward granter
 *   Quest.Debug.Give <ItemTag> <Count>                       changes the stand-in inventory (negative removes)
 */
namespace QuestConsole
{
	/** The stand-in bridge, kept alive here while registered. */
	static TStrongObjectPtr<UQuestDebugBridge> TestBridge;

	/** The world's game instance subsystem of type T, or null outside a running game. */
	template <typename T>
	static T* GetSubsystem(const UWorld* const World)
	{
		const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<T>() : nullptr;
	}

	/** The tag named Name, or an invalid tag (with a warning) if no such tag exists. */
	static FGameplayTag ParseTag(const FString& Name)
	{
		if (Name.IsEmpty() || Name.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return FGameplayTag();
		}
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name), false);
		if (!Tag.IsValid())
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("No gameplay tag named '%s'."), *Name);
		}
		return Tag;
	}

	/** Display name of a status. */
	static FString StatusName(const EQuestStatus Status)
	{
		return StaticEnum<EQuestStatus>()->GetNameStringByValue(static_cast<int64>(Status));
	}

	static void List(const TArray<FString>& Args, UWorld* const World)
	{
		const UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests)
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Quest.List: only in a running game."));
			return;
		}

		UE_LOG(LogQuestFramework, Display, TEXT("%d quest(s):"), Quests->GetAllDefinitions().Num());
		for (const UQuestDefinition* const Definition : Quests->GetAllDefinitions())
		{
			const FQuestRuntimeState* const State = Quests->FindState(Definition->QuestId);
			if (!State)
			{
				continue;
			}

			FString Line = FString::Printf(TEXT("  %s [%s] %s \"%s\""), *Definition->QuestId.ToString(), *StatusName(State->Status),
				*Definition->Category.ToString(), *Definition->Title.ToString());
			if (State->Status == EQuestStatus::Active && Definition->HasTimeLimit())
			{
				Line += FString::Printf(TEXT(" time left %.0f s"), State->TimeRemainingSeconds);
			}
			if (State->CooldownRemainingSeconds > 0.f)
			{
				Line += FString::Printf(TEXT(" cooldown %.0f s"), State->CooldownRemainingSeconds);
			}
			if (State->TimesCompleted > 0)
			{
				Line += FString::Printf(TEXT(" done x%d"), State->TimesCompleted);
			}
			UE_LOG(LogQuestFramework, Display, TEXT("%s"), *Line);

			for (int32 Index = 0; Index < State->ObjectiveProgress.Num() && Index < Definition->Objectives.Num(); ++Index)
			{
				if (const UQuestObjective* const Objective = Definition->Objectives[Index])
				{
					UE_LOG(LogQuestFramework, Display, TEXT("      - %s"), *Objective->GetProgressText(State->ObjectiveProgress[Index]).ToString());
				}
			}
		}
	}

	static void Accept(const TArray<FString>& Args, UWorld* const World)
	{
		UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests || Args.IsEmpty())
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Usage: Quest.Accept <QuestId>"));
			return;
		}
		FText Reason;
		if (!Quests->Accept(FName(*Args[0]), Reason))
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Quest.Accept %s refused: %s"), *Args[0], *Reason.ToString());
		}
	}

	static void Abandon(const TArray<FString>& Args, UWorld* const World)
	{
		UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests || Args.IsEmpty())
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Usage: Quest.Abandon <QuestId>"));
			return;
		}
		if (!Quests->Abandon(FName(*Args[0])))
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Quest.Abandon %s: the quest is not active."), *Args[0]);
		}
	}

	static void TurnIn(const TArray<FString>& Args, UWorld* const World)
	{
		UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests || Args.IsEmpty())
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Usage: Quest.TurnIn <QuestId>"));
			return;
		}
		FText Reason;
		if (!Quests->TurnIn(FName(*Args[0]), Reason))
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Quest.TurnIn %s refused: %s"), *Args[0], *Reason.ToString());
		}
	}

	static void Event(const TArray<FString>& Args, UWorld* const World)
	{
		UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests || Args.Num() < 2)
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Usage: Quest.Event <EventTag> <SubjectTag|None> [ContextId] [Amount]"));
			return;
		}

		FQuestEvent QuestEvent;
		QuestEvent.EventTag = ParseTag(Args[0]);
		if (!QuestEvent.EventTag.IsValid())
		{
			return;
		}
		const FGameplayTag Subject = ParseTag(Args[1]);
		if (Subject.IsValid())
		{
			QuestEvent.SubjectTags.AddTag(Subject);
		}
		if (Args.IsValidIndex(2) && !Args[2].Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			QuestEvent.ContextId = FName(*Args[2]);
		}
		if (Args.IsValidIndex(3))
		{
			QuestEvent.Amount = FCString::Atoi(*Args[3]);
		}

		UE_LOG(LogQuestFramework, Display, TEXT("Quest.Event %s subject %s context %s x%d"), *QuestEvent.EventTag.ToString(),
			*QuestEvent.SubjectTags.ToStringSimple(), *QuestEvent.ContextId.ToString(), QuestEvent.Amount);
		Quests->ReportEvent(QuestEvent);
	}

	static void BountyRefresh(const TArray<FString>& Args, UWorld* const World)
	{
		if (UBountyBoardSubsystem* const Board = GetSubsystem<UBountyBoardSubsystem>(World))
		{
			Board->Refresh();
		}
	}

	static void BountyList(const TArray<FString>& Args, UWorld* const World)
	{
		const UBountyBoardSubsystem* const Board = GetSubsystem<UBountyBoardSubsystem>(World);
		if (!Board)
		{
			return;
		}

		const TArray<UQuestDefinition*> Offers = Board->GetOffers();
		UE_LOG(LogQuestFramework, Display, TEXT("%d bounty offer(s), next seed %d:"), Offers.Num(), Board->GetNextSeed());
		for (const UQuestDefinition* const Offer : Offers)
		{
			FString Rewards;
			for (const FQuestReward& Reward : Offer->Rewards)
			{
				Rewards += FString::Printf(TEXT(" %d x %s"), Reward.Amount, *Reward.RewardTag.ToString());
			}
			UE_LOG(LogQuestFramework, Display, TEXT("  %s \"%s\"%s; reward%s"), *Offer->QuestId.ToString(), *Offer->Title.ToString(),
				Offer->HasTimeLimit() ? *FString::Printf(TEXT(" (%.0f s)"), Offer->TimeLimitSeconds) : TEXT(""), *Rewards);
		}
	}

	static void UseTestBridge(const TArray<FString>& Args, UWorld* const World)
	{
		UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests)
		{
			return;
		}
		if (!TestBridge.IsValid())
		{
			TestBridge.Reset(NewObject<UQuestDebugBridge>());
		}
		Quests->SetInventoryProvider(TestBridge.Get());
		Quests->SetRewardGranter(TestBridge.Get());
		UE_LOG(LogQuestFramework, Display, TEXT("Test bridge registered: Quest.Debug.Give fills its inventory; rewards are logged."));
	}

	static void Give(const TArray<FString>& Args, UWorld* const World)
	{
		UQuestSubsystem* const Quests = GetSubsystem<UQuestSubsystem>(World);
		if (!Quests || !TestBridge.IsValid() || Args.Num() < 2)
		{
			UE_LOG(LogQuestFramework, Warning, TEXT("Usage: Quest.Debug.Give <ItemTag> <Count>, after Quest.Debug.UseTestBridge."));
			return;
		}
		const FGameplayTag Item = ParseTag(Args[0]);
		if (!Item.IsValid())
		{
			return;
		}

		const int32 Delta = FCString::Atoi(*Args[1]);
		const int32 Total = TestBridge->AddItems(Item, Delta);
		UE_LOG(LogQuestFramework, Display, TEXT("Test inventory: %s = %d"), *Item.ToString(), Total);

		// Gains are reported like the game would; losses only refresh the inventory objectives.
		if (Delta > 0)
		{
			FQuestEvent Collected;
			Collected.EventTag = QuestTags::Event_ItemCollected;
			Collected.SubjectTags.AddTag(Item);
			Collected.Amount = Delta;
			Quests->ReportEvent(Collected);
		}
		else
		{
			Quests->RefreshInventoryObjectives();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs ListCommand(TEXT("Quest.List"),
		TEXT("Logs every quest with its status, objective progress and time left."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&List));

	static FAutoConsoleCommandWithWorldAndArgs AcceptCommand(TEXT("Quest.Accept"),
		TEXT("Quest.Accept <QuestId>: accepts a quest."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Accept));

	static FAutoConsoleCommandWithWorldAndArgs AbandonCommand(TEXT("Quest.Abandon"),
		TEXT("Quest.Abandon <QuestId>: abandons an active quest."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Abandon));

	static FAutoConsoleCommandWithWorldAndArgs TurnInCommand(TEXT("Quest.TurnIn"),
		TEXT("Quest.TurnIn <QuestId>: turns in a quest that is ready."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TurnIn));

	static FAutoConsoleCommandWithWorldAndArgs EventCommand(TEXT("Quest.Event"),
		TEXT("Quest.Event <EventTag> <SubjectTag|None> [ContextId] [Amount]: reports a quest event by hand."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Event));

	static FAutoConsoleCommandWithWorldAndArgs BountyRefreshCommand(TEXT("Quest.Bounty.Refresh"),
		TEXT("Refreshes the bounty board."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BountyRefresh));

	static FAutoConsoleCommandWithWorldAndArgs BountyListCommand(TEXT("Quest.Bounty.List"),
		TEXT("Logs the bounty board's offers."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BountyList));

	static FAutoConsoleCommandWithWorldAndArgs UseTestBridgeCommand(TEXT("Quest.Debug.UseTestBridge"),
		TEXT("Registers a stand-in inventory and reward granter, for testing without a game bridge."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UseTestBridge));

	static FAutoConsoleCommandWithWorldAndArgs GiveCommand(TEXT("Quest.Debug.Give"),
		TEXT("Quest.Debug.Give <ItemTag> <Count>: changes the test bridge's inventory (negative removes)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Give));
}

#endif
