#include "UI/Hub/HubQuestsTabWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Bounties/BountyBoardSubsystem.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/QuestSubsystem.h"
#include "TimerManager.h"
#include "UI/Hub/HubPanelWidget.h"
#include "UI/Hub/QuestDetailsWidget.h"
#include "UI/Hub/QuestListWidget.h"

namespace HubQuestsTab
{
	constexpr float ListShare = 0.45f;
	constexpr float PanelGap = 12.f;
	constexpr float CooldownTickSeconds = 1.f;
}

void UHubQuestsTabWidget::BuildContent()
{
	UHorizontalBox* const Root = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = Root;

	const auto AddPanel = [this, Root](UWidget* const Content, const float Share, const FMargin& SlotPadding)
	{
		UOverlay* const Overlay = WidgetTree->ConstructWidget<UOverlay>();
		UHorizontalBoxSlot* const OverlaySlot = Root->AddChildToHorizontalBox(Overlay);
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Share;
		OverlaySlot->SetSize(Size);
		OverlaySlot->SetPadding(SlotPadding);

		UHubPanelWidget* const Panel = WidgetTree->ConstructWidget<UHubPanelWidget>(UHubPanelWidget::StaticClass());
		UOverlaySlot* const PanelSlot = Overlay->AddChildToOverlay(Panel);
		PanelSlot->SetHorizontalAlignment(HAlign_Fill);
		PanelSlot->SetVerticalAlignment(VAlign_Fill);

		UOverlaySlot* const ContentSlot = Overlay->AddChildToOverlay(Content);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(4.f));
	};

	List = WidgetTree->ConstructWidget<UQuestListWidget>(UQuestListWidget::StaticClass());
	Details = WidgetTree->ConstructWidget<UQuestDetailsWidget>(UQuestDetailsWidget::StaticClass());
	AddPanel(List, HubQuestsTab::ListShare, FMargin(0.f, 0.f, HubQuestsTab::PanelGap * 0.5f, 0.f));
	AddPanel(Details, 1.f - HubQuestsTab::ListShare, FMargin(HubQuestsTab::PanelGap * 0.5f, 0.f, 0.f, 0.f));
}

void UHubQuestsTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	List->OnQuestSelected.AddUObject(this, &UHubQuestsTabWidget::HandleQuestSelected);

	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->OnQuestStatusChanged.AddUniqueDynamic(this, &UHubQuestsTabWidget::HandleQuestStatusChanged);
		Quests->OnObjectiveProgress.AddUniqueDynamic(this, &UHubQuestsTabWidget::HandleObjectiveProgress);
		Quests->OnQuestTimeChanged.AddUniqueDynamic(this, &UHubQuestsTabWidget::HandleQuestTimeChanged);
		Quests->OnQuestUnregistered.AddUniqueDynamic(this, &UHubQuestsTabWidget::HandleQuestUnregistered);
		BoundQuests = Quests;
	}

	const UGameInstance* const GameInstance = IsDesignTime() ? nullptr : GetGameInstance();
	if (UBountyBoardSubsystem* const Board = GameInstance ? GameInstance->GetSubsystem<UBountyBoardSubsystem>() : nullptr)
	{
		Board->OnBoardRefreshed.AddUniqueDynamic(this, &UHubQuestsTabWidget::HandleBoardRefreshed);
		BoundBoard = Board;
	}

	RebuildAll();
}

void UHubQuestsTabWidget::NativeDestruct()
{
	if (UQuestSubsystem* const Quests = BoundQuests.Get())
	{
		Quests->OnQuestStatusChanged.RemoveDynamic(this, &UHubQuestsTabWidget::HandleQuestStatusChanged);
		Quests->OnObjectiveProgress.RemoveDynamic(this, &UHubQuestsTabWidget::HandleObjectiveProgress);
		Quests->OnQuestTimeChanged.RemoveDynamic(this, &UHubQuestsTabWidget::HandleQuestTimeChanged);
		Quests->OnQuestUnregistered.RemoveDynamic(this, &UHubQuestsTabWidget::HandleQuestUnregistered);
	}
	BoundQuests.Reset();

	if (UBountyBoardSubsystem* const Board = BoundBoard.Get())
	{
		Board->OnBoardRefreshed.RemoveDynamic(this, &UHubQuestsTabWidget::HandleBoardRefreshed);
	}
	BoundBoard.Reset();

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimer);
	}
	List->OnQuestSelected.RemoveAll(this);

	Super::NativeDestruct();
}

void UHubQuestsTabWidget::HandleQuestStatusChanged(const FName QuestId, const EQuestStatus NewStatus)
{
	RebuildAll();
}

void UHubQuestsTabWidget::HandleObjectiveProgress(const FName QuestId, const int32 ObjectiveIndex, const int32 Current,
	const int32 Required)
{
	List->RefreshQuest(QuestId);
	if (Details->GetQuestId() == QuestId)
	{
		Details->Refresh();
	}
}

void UHubQuestsTabWidget::HandleQuestTimeChanged(const FName QuestId, const float SecondsRemaining)
{
	List->RefreshQuest(QuestId);
	if (Details->GetQuestId() == QuestId)
	{
		Details->RefreshTimer();
	}
}

void UHubQuestsTabWidget::HandleQuestUnregistered(const FName QuestId)
{
	RebuildAll();
}

void UHubQuestsTabWidget::HandleBoardRefreshed()
{
	RebuildAll();
}

void UHubQuestsTabWidget::HandleQuestSelected(const FName QuestId)
{
	Details->ShowQuest(QuestId);
}

void UHubQuestsTabWidget::HandleCooldownTick()
{
	List->RefreshCoolingRows();

	// The refusal reason under Accept counts the cooldown down too.
	const UQuestSubsystem* const Quests = BoundQuests.Get();
	const FQuestRuntimeState* const State = Quests ? Quests->FindState(Details->GetQuestId()) : nullptr;
	if (State && State->CooldownRemainingSeconds > 0.f)
	{
		Details->Refresh();
	}
}

void UHubQuestsTabWidget::RebuildAll()
{
	// Rebuild selects a row and broadcasts when the selection changes, which shows it in the details.
	const FName Before = List->GetSelectedQuest();
	List->Rebuild();
	if (List->GetSelectedQuest() == Before)
	{
		Details->Refresh();
	}
	UpdateCooldownTimer();
}

void UHubQuestsTabWidget::UpdateCooldownTimer()
{
	UWorld* const World = GetWorld();
	if (!World || IsDesignTime())
	{
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();
	if (List->HasCoolingRows())
	{
		if (!Timers.IsTimerActive(CooldownTimer))
		{
			Timers.SetTimer(CooldownTimer, FTimerDelegate::CreateUObject(this, &UHubQuestsTabWidget::HandleCooldownTick),
				HubQuestsTab::CooldownTickSeconds, true);
		}
	}
	else
	{
		Timers.ClearTimer(CooldownTimer);
	}
}
