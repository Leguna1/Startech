#include "UI/Hub/QuestListWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Bounties/BountyBoardSubsystem.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Runtime/QuestSubsystem.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/QuestListRowWidget.h"
#include "UI/Quest/QuestUiFormat.h"

#define LOCTEXT_NAMESPACE "QuestList"

namespace QuestList
{
	constexpr float Inset = 8.f;
	constexpr float HeaderTop = 14.f;
	constexpr float HeaderBottom = 4.f;
	constexpr float RowGap = 2.f;
}

void UQuestListWidget::BuildContent()
{
	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	WidgetTree->RootWidget = Scroll;

	Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Content);
}

void UQuestListWidget::Rebuild()
{
	for (UQuestListRowWidget* const Row : Rows)
	{
		Row->OnClicked.RemoveAll(this);
	}
	Content->ClearChildren();
	Rows.Reset();

	const UQuestSubsystem* const Quests = GetQuests();
	if (!Quests)
	{
		Select(NAME_None);
		return;
	}

	TArray<FName> Active;
	TArray<FName> Available;
	TArray<FName> Completed;
	for (const UQuestDefinition* const Definition : Quests->GetAllDefinitions())
	{
		const FQuestRuntimeState* const State = Definition ? Quests->FindState(Definition->QuestId) : nullptr;
		if (!State)
		{
			continue;
		}

		if (State->Status == EQuestStatus::Active || State->Status == EQuestStatus::ReadyToTurnIn)
		{
			Active.Add(Definition->QuestId);
		}
		else if (Definition->IsGenerated())
		{
			// Offers are listed from the bounty board below.
		}
		else if (State->Status == EQuestStatus::Available || FQuestUiFormat::IsCoolingDown(*State))
		{
			Available.Add(Definition->QuestId);
		}
		else if (State->Status == EQuestStatus::Completed)
		{
			Completed.Add(Definition->QuestId);
		}
	}

	if (!Active.IsEmpty())
	{
		AddHeader(LOCTEXT("Active", "Active"));
		for (const FName Id : Active)
		{
			AddRow(Id);
		}
	}

	if (!Available.IsEmpty())
	{
		AddHeader(LOCTEXT("Available", "Available"));
		for (const FName Id : Available)
		{
			AddRow(Id);
		}
	}

	AddHeader(LOCTEXT("BountyBoard", "Bounty board · refreshes when you dock"));
	const UGameInstance* const GameInstance = GetGameInstance();
	const UBountyBoardSubsystem* const Board = GameInstance ? GameInstance->GetSubsystem<UBountyBoardSubsystem>() : nullptr;
	const TArray<UQuestDefinition*> Offers = Board ? Board->GetOffers() : TArray<UQuestDefinition*>();
	for (const UQuestDefinition* const Offer : Offers)
	{
		AddRow(Offer->QuestId);
	}
	if (Offers.IsEmpty())
	{
		AddNote(LOCTEXT("NoOffers", "No offers right now"));
	}

	if (!Completed.IsEmpty())
	{
		AddCompletedHeader(Completed.Num());
		if (bShowCompleted)
		{
			for (const FName Id : Completed)
			{
				AddRow(Id);
			}
		}
	}

	// Keep the selection while it is listed, else the first row.
	const bool bStillListed = Rows.ContainsByPredicate([this](const UQuestListRowWidget* Row)
	{
		return Row->GetQuestId() == SelectedQuest;
	});
	Select(bStillListed ? SelectedQuest : Rows.IsEmpty() ? NAME_None : Rows[0]->GetQuestId());
}

void UQuestListWidget::RefreshQuest(const FName QuestId)
{
	for (UQuestListRowWidget* const Row : Rows)
	{
		if (Row->GetQuestId() == QuestId)
		{
			Row->Refresh();
		}
	}
}

void UQuestListWidget::RefreshCoolingRows()
{
	for (UQuestListRowWidget* const Row : Rows)
	{
		if (Row->IsCoolingDown())
		{
			Row->Refresh();
		}
	}
}

bool UQuestListWidget::HasCoolingRows() const
{
	return Rows.ContainsByPredicate([](const UQuestListRowWidget* Row) { return Row->IsCoolingDown(); });
}

void UQuestListWidget::AddHeader(const FText& Text)
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	UTextBlock* const Header = MakeText(Text, HudStyle.HubGroupFontSize, HudStyle.TextSecondaryColor);
	UVerticalBoxSlot* const HeaderSlot = Content->AddChildToVerticalBox(Header);
	HeaderSlot->SetPadding(FMargin(QuestList::Inset, QuestList::HeaderTop, QuestList::Inset, QuestList::HeaderBottom));
}

void UQuestListWidget::AddNote(const FText& Text)
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	UTextBlock* const Note = MakeText(Text, HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	UVerticalBoxSlot* const NoteSlot = Content->AddChildToVerticalBox(Note);
	NoteSlot->SetPadding(FMargin(QuestList::Inset + 10.f, 2.f, QuestList::Inset, 2.f));
}

void UQuestListWidget::AddCompletedHeader(const int32 Count)
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UHorizontalBox* const Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* const HeaderSlot = Content->AddChildToVerticalBox(Header);
	HeaderSlot->SetPadding(FMargin(QuestList::Inset, QuestList::HeaderTop - 6.f, QuestList::Inset, 0.f));

	UTextBlock* const Label = MakeText(FText::Format(LOCTEXT("Completed", "Completed ({0})"), FText::AsNumber(Count)),
		HudStyle.HubGroupFontSize, HudStyle.TextSecondaryColor);
	UHorizontalBoxSlot* const LabelSlot = Header->AddChildToHorizontalBox(Label);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	UHubButtonWidget* const Toggle = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	Toggle->SetQuiet(true);
	Toggle->SetLabel(bShowCompleted ? LOCTEXT("Hide", "Hide") : LOCTEXT("Show", "Show"));
	Toggle->OnClicked.AddUObject(this, &UQuestListWidget::HandleToggleCompleted);
	Header->AddChildToHorizontalBox(Toggle)->SetVerticalAlignment(VAlign_Center);
}

void UQuestListWidget::AddRow(const FName QuestId)
{
	UQuestListRowWidget* const Row = WidgetTree->ConstructWidget<UQuestListRowWidget>(UQuestListRowWidget::StaticClass());
	UVerticalBoxSlot* const RowSlot = Content->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(QuestList::Inset, 0.f, QuestList::Inset, QuestList::RowGap));
	Row->Setup(QuestId);
	Row->SetSelected(QuestId == SelectedQuest);
	Row->OnClicked.AddUObject(this, &UQuestListWidget::HandleRowClicked);
	Rows.Add(Row);
}

void UQuestListWidget::Select(const FName QuestId)
{
	for (UQuestListRowWidget* const Row : Rows)
	{
		Row->SetSelected(Row->GetQuestId() == QuestId);
	}

	if (SelectedQuest != QuestId)
	{
		SelectedQuest = QuestId;
		OnQuestSelected.Broadcast(QuestId);
	}
}

void UQuestListWidget::HandleRowClicked(const FName QuestId)
{
	Select(QuestId);
}

void UQuestListWidget::HandleToggleCompleted()
{
	bShowCompleted = !bShowCompleted;
	Rebuild();
}

#undef LOCTEXT_NAMESPACE
