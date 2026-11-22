#include "UI/Hub/QuestDetailsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Definitions/QuestDefinition.h"
#include "Runtime/QuestSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UI/Hub/QuestObjectiveRowWidget.h"
#include "UI/Quest/QuestUiFormat.h"

#define LOCTEXT_NAMESPACE "QuestDetails"

namespace QuestDetails
{
	constexpr float Inset = 20.f;
	constexpr float SectionGap = 16.f;
	constexpr float LineGap = 4.f;
	constexpr float ButtonGap = 16.f;
	constexpr float BadgeFillOpacity = 0.2f;
}

void UQuestDetailsWidget::BuildContent()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();

	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	WidgetTree->RootWidget = Scroll;

	UVerticalBox* const Page = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Page);

	EmptyText = MakeText(LOCTEXT("Empty", "Select a quest"), HudStyle.BodyFontSize, HudStyle.TextSecondaryColor);
	Page->AddChildToVerticalBox(EmptyText)->SetPadding(FMargin(QuestDetails::Inset));

	Body = WidgetTree->ConstructWidget<UVerticalBox>();
	Page->AddChildToVerticalBox(Body)->SetPadding(FMargin(QuestDetails::Inset));

	// Header: the title and the category badge.
	UHorizontalBox* const Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Body->AddChildToVerticalBox(Header);

	TitleText = MakeText(FText::GetEmpty(), HudStyle.HubTitleFontSize, HudStyle.TextColor, true);
	TitleText->SetAutoWrapText(true);
	UHorizontalBoxSlot* const TitleSlot = Header->AddChildToHorizontalBox(TitleText);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	Badge = WidgetTree->ConstructWidget<UBorder>();
	Badge->SetPadding(FMargin(8.f, 2.f));
	BadgeText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextColor, true);
	Badge->SetContent(BadgeText);
	UHorizontalBoxSlot* const BadgeSlot = Header->AddChildToHorizontalBox(Badge);
	BadgeSlot->SetVerticalAlignment(VAlign_Center);
	BadgeSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

	DescriptionText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.TextColor);
	DescriptionText->SetAutoWrapText(true);
	Body->AddChildToVerticalBox(DescriptionText)->SetPadding(FMargin(0.f, QuestDetails::SectionGap * 0.5f, 0.f, 0.f));

	AddSectionHeader(LOCTEXT("Objectives", "Objectives"));
	ObjectivesBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Body->AddChildToVerticalBox(ObjectivesBox);

	TimeText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.TextColor);
	Body->AddChildToVerticalBox(TimeText)->SetPadding(FMargin(0.f, QuestDetails::SectionGap * 0.5f, 0.f, 0.f));

	AddSectionHeader(LOCTEXT("Rewards", "Rewards"));
	RewardsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Body->AddChildToVerticalBox(RewardsBox);

	HandoverText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	Body->AddChildToVerticalBox(HandoverText)->SetPadding(FMargin(0.f, QuestDetails::LineGap, 0.f, 0.f));

	// The main button and Abandon.
	ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Body->AddChildToVerticalBox(ActionRow)->SetPadding(FMargin(0.f, QuestDetails::SectionGap * 1.5f, 0.f, 0.f));

	MainButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	ActionRow->AddChildToHorizontalBox(MainButton)->SetVerticalAlignment(VAlign_Center);

	AbandonButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	AbandonButton->SetQuiet(true);
	AbandonButton->SetLabel(LOCTEXT("Abandon", "Abandon"));
	UHorizontalBoxSlot* const AbandonSlot = ActionRow->AddChildToHorizontalBox(AbandonButton);
	AbandonSlot->SetVerticalAlignment(VAlign_Center);
	AbandonSlot->SetPadding(FMargin(QuestDetails::ButtonGap, 0.f, 0.f, 0.f));

	ReasonText = MakeText(FText::GetEmpty(), HudStyle.SmallFontSize, HudStyle.TextSecondaryColor);
	ReasonText->SetAutoWrapText(true);
	Body->AddChildToVerticalBox(ReasonText)->SetPadding(FMargin(0.f, QuestDetails::LineGap, 0.f, 0.f));

	// The Abandon confirmation, in place of the buttons while open.
	ConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Body->AddChildToVerticalBox(ConfirmBox)->SetPadding(FMargin(0.f, QuestDetails::SectionGap * 1.5f, 0.f, 0.f));

	ConfirmText = MakeText(FText::GetEmpty(), HudStyle.BodyFontSize, HudStyle.TextColor);
	ConfirmText->SetAutoWrapText(true);
	ConfirmBox->AddChildToVerticalBox(ConfirmText);

	UHorizontalBox* const ConfirmRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	ConfirmBox->AddChildToVerticalBox(ConfirmRow)->SetPadding(FMargin(0.f, QuestDetails::LineGap * 2.f, 0.f, 0.f));

	ConfirmAbandonButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	ConfirmAbandonButton->SetLabel(LOCTEXT("ConfirmAbandon", "Abandon"));
	ConfirmRow->AddChildToHorizontalBox(ConfirmAbandonButton)->SetVerticalAlignment(VAlign_Center);

	KeepButton = WidgetTree->ConstructWidget<UHubButtonWidget>(UHubButtonWidget::StaticClass());
	KeepButton->SetQuiet(true);
	KeepButton->SetLabel(LOCTEXT("Keep", "Keep"));
	UHorizontalBoxSlot* const KeepSlot = ConfirmRow->AddChildToHorizontalBox(KeepButton);
	KeepSlot->SetVerticalAlignment(VAlign_Center);
	KeepSlot->SetPadding(FMargin(QuestDetails::ButtonGap, 0.f, 0.f, 0.f));

	Refresh();
}

UTextBlock* UQuestDetailsWidget::AddSectionHeader(const FText& Text)
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	UTextBlock* const Header = MakeText(Text, HudStyle.HubGroupFontSize, HudStyle.TextSecondaryColor);
	Body->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, QuestDetails::SectionGap, 0.f, QuestDetails::LineGap));
	return Header;
}

void UQuestDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MainButton->OnClicked.AddUObject(this, &UQuestDetailsWidget::HandleMainClicked);
	AbandonButton->OnClicked.AddUObject(this, &UQuestDetailsWidget::HandleAbandonClicked);
	ConfirmAbandonButton->OnClicked.AddUObject(this, &UQuestDetailsWidget::HandleConfirmAbandonClicked);
	KeepButton->OnClicked.AddUObject(this, &UQuestDetailsWidget::HandleKeepClicked);
}

void UQuestDetailsWidget::NativeDestruct()
{
	MainButton->OnClicked.RemoveAll(this);
	AbandonButton->OnClicked.RemoveAll(this);
	ConfirmAbandonButton->OnClicked.RemoveAll(this);
	KeepButton->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UQuestDetailsWidget::ShowQuest(const FName InQuestId)
{
	QuestId = InQuestId;
	bConfirming = false;
	Refresh();
}

void UQuestDetailsWidget::Refresh()
{
	if (!Body)
	{
		return;
	}

	const UQuestSubsystem* const Quests = GetQuests();
	const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(QuestId) : nullptr;
	const FQuestRuntimeState* const State = Quests ? Quests->FindState(QuestId) : nullptr;

	EmptyText->SetVisibility(Definition && State ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Body->SetVisibility(Definition && State ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!Definition || !State)
	{
		MainAction = EMainAction::None;
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();

	// Header and description.
	TitleText->SetText(Definition->Title);
	const FLinearColor CategoryColor = FQuestUiFormat::CategoryColor(*Definition, HudStyle);
	BadgeText->SetText(FQuestUiFormat::CategoryName(*Definition));
	BadgeText->SetColorAndOpacity(FSlateColor(CategoryColor));
	Badge->SetBrushColor(FHudDraw::WithOpacity(CategoryColor, QuestDetails::BadgeFillOpacity));
	DescriptionText->SetText(Definition->Description);
	DescriptionText->SetVisibility(Definition->Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// Objectives: one row each, reused when the count matches.
	const int32 ObjectiveCount = Definition->Objectives.Num();
	if (ObjectivesBox->GetChildrenCount() != ObjectiveCount)
	{
		ObjectivesBox->ClearChildren();
		for (int32 Index = 0; Index < ObjectiveCount; ++Index)
		{
			UQuestObjectiveRowWidget* const Row = WidgetTree->ConstructWidget<UQuestObjectiveRowWidget>(
				UQuestObjectiveRowWidget::StaticClass());
			ObjectivesBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, QuestDetails::LineGap));
		}
	}
	for (int32 Index = 0; Index < ObjectiveCount; ++Index)
	{
		if (UQuestObjectiveRowWidget* const Row = Cast<UQuestObjectiveRowWidget>(ObjectivesBox->GetChildAt(Index)))
		{
			Row->SetObjective(FQuestUiFormat::ObjectiveText(*Definition, *State, Index),
				FQuestUiFormat::GetProgress(*State, Index), FQuestUiFormat::GetRequired(*Definition, Index));
		}
	}

	RefreshTimer();

	// Rewards.
	RewardsBox->ClearChildren();
	for (const FQuestReward& Reward : Definition->Rewards)
	{
		const bool bCredits = FQuestUiFormat::IsCreditsReward(Reward);
		RewardsBox->AddChildToVerticalBox(MakeText(FQuestUiFormat::RewardText(Reward), HudStyle.BodyFontSize,
			bCredits ? HudStyle.CreditsColor : HudStyle.TextColor, bCredits));
	}

	const FText Handover = FQuestUiFormat::HandoverText(*Definition);
	HandoverText->SetText(Handover);
	HandoverText->SetVisibility(Handover.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// The main button, with the refusal reason from the same checks Accept and TurnIn run.
	const bool bAccepted = State->Status == EQuestStatus::Active || State->Status == EQuestStatus::ReadyToTurnIn;
	FText Reason;
	bool bEnabled = false;
	MainAction = EMainAction::None;

	if (State->Status == EQuestStatus::ReadyToTurnIn)
	{
		MainAction = EMainAction::TurnIn;
		MainButton->SetLabel(LOCTEXT("TurnIn", "Turn in"));
		bEnabled = Quests->CanTurnIn(QuestId, Reason);
	}
	else if (State->Status == EQuestStatus::Active)
	{
		MainButton->SetLabel(LOCTEXT("InProgress", "In progress"));
	}
	else if (State->Status == EQuestStatus::Completed && !FQuestUiFormat::IsCoolingDown(*State))
	{
		Reason = LOCTEXT("Completed", "Completed");
	}
	else
	{
		MainAction = EMainAction::Accept;
		MainButton->SetLabel(LOCTEXT("Accept", "Accept"));
		bEnabled = Quests->CanAccept(QuestId, Reason);
	}

	const bool bShowMain = MainAction != EMainAction::None || State->Status == EQuestStatus::Active;
	MainButton->SetVisibility(bShowMain ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MainButton->SetButtonEnabled(bEnabled);
	AbandonButton->SetVisibility(bAccepted ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	const bool bShowReason = !bEnabled && !Reason.IsEmpty();
	ReasonText->SetText(bShowReason ? Reason : FText::GetEmpty());
	ReasonText->SetVisibility(bShowReason ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// The confirmation replaces the buttons while open, and closes if the quest is no longer active.
	bConfirming = bConfirming && bAccepted;
	ConfirmText->SetText(FText::Format(LOCTEXT("ConfirmText", "Abandon {0}? Progress is lost."), Definition->Title));
	ConfirmBox->SetVisibility(bConfirming ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	ActionRow->SetVisibility(bConfirming ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (bConfirming)
	{
		ReasonText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UQuestDetailsWidget::RefreshTimer()
{
	if (!TimeText)
	{
		return;
	}

	const UQuestSubsystem* const Quests = GetQuests();
	const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(QuestId) : nullptr;
	const FQuestRuntimeState* const State = Quests ? Quests->FindState(QuestId) : nullptr;
	const FText Time = Definition && State ? FQuestUiFormat::TimeLimitText(*Definition, *State) : FText::GetEmpty();

	TimeText->SetText(Time);
	TimeText->SetVisibility(Time.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Definition && State)
	{
		const UStartechHudStyle& HudStyle = GetHudStyle();
		TimeText->SetColorAndOpacity(FSlateColor(FQuestUiFormat::IsTimerRunning(*Definition, *State)
			? FQuestUiFormat::TimerColor(State->TimeRemainingSeconds, HudStyle) : HudStyle.TextSecondaryColor));
	}
}

void UQuestDetailsWidget::HandleMainClicked()
{
	UQuestSubsystem* const Quests = GetQuests();
	if (!Quests)
	{
		return;
	}

	// The subsystem's events refresh the hub; a refusal shows its reason at once.
	FText Reason;
	const bool bDone = MainAction == EMainAction::Accept ? Quests->Accept(QuestId, Reason)
		: MainAction == EMainAction::TurnIn ? Quests->TurnIn(QuestId, Reason) : true;
	if (!bDone)
	{
		Refresh();
	}
}

void UQuestDetailsWidget::HandleAbandonClicked()
{
	bConfirming = true;
	Refresh();
}

void UQuestDetailsWidget::HandleConfirmAbandonClicked()
{
	bConfirming = false;
	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->Abandon(QuestId);
	}
	Refresh();
}

void UQuestDetailsWidget::HandleKeepClicked()
{
	bConfirming = false;
	Refresh();
}

#undef LOCTEXT_NAMESPACE
