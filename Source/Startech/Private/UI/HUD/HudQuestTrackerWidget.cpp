#include "UI/HUD/HudQuestTrackerWidget.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Data/SectorCatalog.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HudFormat.h"
#include "Runtime/QuestSubsystem.h"
#include "Station.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"
#include "UI/Quest/QuestUiFormat.h"

#define LOCTEXT_NAMESPACE "HudQuestTracker"

namespace QuestTracker
{
	constexpr float Padding = 12.f;
	constexpr float LineGap = 2.f;
	constexpr float EntryGap = 10.f;
}

UHudQuestTrackerWidget::UHudQuestTrackerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 4;
}

void UHudQuestTrackerWidget::NativeConstruct()
{
	const UGameInstance* const GameInstance = IsDesignTime() ? nullptr : GetGameInstance();
	if (UQuestSubsystem* const Quests = GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr)
	{
		Quests->OnQuestStatusChanged.AddUniqueDynamic(this, &UHudQuestTrackerWidget::HandleQuestStatusChanged);
		Quests->OnObjectiveProgress.AddUniqueDynamic(this, &UHudQuestTrackerWidget::HandleObjectiveProgress);
		Quests->OnQuestTimeChanged.AddUniqueDynamic(this, &UHudQuestTrackerWidget::HandleQuestTimeChanged);
		Quests->OnQuestUnregistered.AddUniqueDynamic(this, &UHudQuestTrackerWidget::HandleQuestUnregistered);
		BoundQuests = Quests;
	}

	// After binding: Super syncs, which reads the quests.
	Super::NativeConstruct();
}

void UHudQuestTrackerWidget::NativeDestruct()
{
	if (UQuestSubsystem* const Quests = BoundQuests.Get())
	{
		Quests->OnQuestStatusChanged.RemoveDynamic(this, &UHudQuestTrackerWidget::HandleQuestStatusChanged);
		Quests->OnObjectiveProgress.RemoveDynamic(this, &UHudQuestTrackerWidget::HandleObjectiveProgress);
		Quests->OnQuestTimeChanged.RemoveDynamic(this, &UHudQuestTrackerWidget::HandleQuestTimeChanged);
		Quests->OnQuestUnregistered.RemoveDynamic(this, &UHudQuestTrackerWidget::HandleQuestUnregistered);
	}
	BoundQuests.Reset();
	HandlePawnChanged(GetBoundPawn(), nullptr);

	Super::NativeDestruct();
}

void UHudQuestTrackerWidget::HandleStyleChanged()
{
	const UStartechHudStyle& HudStyle = GetHudStyle();
	Panel.Initialize(this, HudStyle, FHudDraw::WithOpacity(HudStyle.PlayerColor, 0.35f));
}

void UHudQuestTrackerWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	if (AStartechShipPawn* const OldShip = BoundShip.Get())
	{
		OldShip->OnDockedChanged.RemoveDynamic(this, &UHudQuestTrackerWidget::HandleDockedChanged);
	}

	Super::HandlePawnChanged(OldPawn, NewPawn);

	AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn);
	BoundShip = Ship;
	if (Ship)
	{
		Ship->OnDockedChanged.AddUniqueDynamic(this, &UHudQuestTrackerWidget::HandleDockedChanged);
	}
}

void UHudQuestTrackerWidget::RefreshFromPawn()
{
	const AStartechShipPawn* const Ship = BoundShip.Get();
	if (const AStation* const Station = Ship ? Ship->GetDockedStation() : nullptr)
	{
		StationName = Station->GetDisplayName();
	}
	RebuildEntries();
}

void UHudQuestTrackerWidget::HandleQuestStatusChanged(const FName QuestId, const EQuestStatus NewStatus)
{
	RebuildEntries();
}

void UHudQuestTrackerWidget::HandleObjectiveProgress(const FName QuestId, const int32 ObjectiveIndex, const int32 Current,
	const int32 Required)
{
	for (FTrackerEntry& Entry : Entries)
	{
		if (Entry.QuestId == QuestId)
		{
			FillEntry(Entry);
			RequestRepaint();
		}
	}
}

void UHudQuestTrackerWidget::HandleQuestTimeChanged(const FName QuestId, const float SecondsRemaining)
{
	HandleObjectiveProgress(QuestId, INDEX_NONE, 0, 0);
}

void UHudQuestTrackerWidget::HandleQuestUnregistered(const FName QuestId)
{
	RebuildEntries();
}

void UHudQuestTrackerWidget::HandleDockedChanged(const bool bDocked, const FText InStationName)
{
	if (bDocked && !InStationName.IsEmpty())
	{
		StationName = InStationName;
		RebuildEntries();
	}
}

void UHudQuestTrackerWidget::RebuildEntries()
{
	Entries.Reset();

	const UQuestSubsystem* const Quests = BoundQuests.Get();
	if (Quests)
	{
		// Timed first, then story, then bounties; registry order within each.
		TArray<const UQuestDefinition*> Tracked;
		for (const UQuestDefinition* const Definition : Quests->GetAllDefinitions())
		{
			const FQuestRuntimeState* const State = Definition ? Quests->FindState(Definition->QuestId) : nullptr;
			if (State && (State->Status == EQuestStatus::Active || State->Status == EQuestStatus::ReadyToTurnIn))
			{
				Tracked.Add(Definition);
			}
		}
		Tracked.StableSort([](const UQuestDefinition& A, const UQuestDefinition& B)
		{
			return FQuestUiFormat::CategoryRank(A) < FQuestUiFormat::CategoryRank(B);
		});

		const int32 Count = FMath::Min(Tracked.Num(), GetHudStyle().TrackerMaxEntries);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FTrackerEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.QuestId = Tracked[Index]->QuestId;
			Entry.Title = Tracked[Index]->Title.ToString();
			FillEntry(Entry);
		}
	}
	RequestRepaint();
}

void UHudQuestTrackerWidget::FillEntry(FTrackerEntry& Entry) const
{
	const UQuestSubsystem* const Quests = BoundQuests.Get();
	const UQuestDefinition* const Definition = Quests ? Quests->GetDefinition(Entry.QuestId) : nullptr;
	const FQuestRuntimeState* const State = Quests ? Quests->FindState(Entry.QuestId) : nullptr;
	if (!Definition || !State)
	{
		return;
	}

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const bool bTimer = FQuestUiFormat::IsTimerRunning(*Definition, *State);
	Entry.Timer = bTimer ? FHudFormat::Countdown(State->TimeRemainingSeconds).ToString() : FString();
	Entry.TimerColor = bTimer ? FQuestUiFormat::TimerColor(State->TimeRemainingSeconds, HudStyle) : HudStyle.TextColor;

	if (State->Status == EQuestStatus::ReadyToTurnIn)
	{
		Entry.Line = FText::Format(LOCTEXT("Ready", "Ready to turn in at {0}"), GetTurnInPlace()).ToString();
		Entry.LineColor = HudStyle.AdvantageColor;
	}
	else
	{
		Entry.Line = FQuestUiFormat::ObjectiveText(*Definition, *State,
			FQuestUiFormat::FindCurrentObjective(*Definition, *State)).ToString();
		Entry.LineColor = HudStyle.TextSecondaryColor;
	}
}

FText UHudQuestTrackerWidget::GetTurnInPlace() const
{
	if (!StationName.IsEmpty())
	{
		return StationName;
	}

	// Not docked yet this game: the home sector, the only one with a station.
	const FName HomeId = UStartechSettings::Get().HomeSectorId;
	const UWorld* const World = GetWorld();
	const USectorDirectorSubsystem* const Director = World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	const USectorCatalog* const Catalog = Director ? Director->GetCatalog() : nullptr;
	const FSectorDefinition* const Home = Catalog
		? Catalog->Sectors.FindByPredicate([HomeId](const FSectorDefinition& Each) { return Each.SectorId == HomeId; })
		: nullptr;
	return Home && !Home->DisplayName.IsEmpty() ? Home->DisplayName : FText::FromName(HomeId);
}

int32 UHudQuestTrackerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	if (Entries.IsEmpty())
	{
		return BaseLayer;
	}

	const int32 PanelLayer = BaseLayer + 1;
	const int32 TextLayer = BaseLayer + 2;
	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float Width = AllottedGeometry.GetLocalSize().X;
	const FSlateFontInfo TitleFont = UStartechHudStyle::MakeFont(HudStyle.BodyFontSize);
	const FSlateFontInfo LineFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);
	const float TitleHeight = FHudDraw::Measure(TEXT("Ag"), TitleFont).Y;
	const float LineHeight = FHudDraw::Measure(TEXT("Ag"), LineFont).Y;
	const float EntryHeight = TitleHeight + QuestTracker::LineGap + LineHeight;

	// The panel fits the entries.
	const float PanelHeight = QuestTracker::Padding * 2.f + Entries.Num() * EntryHeight
		+ (Entries.Num() - 1) * QuestTracker::EntryGap;
	Panel.Draw(OutDrawElements, PanelLayer, AllottedGeometry, FVector2f::ZeroVector, FVector2f(Width, PanelHeight), HudStyle);

	const float Left = QuestTracker::Padding;
	const float Right = Width - QuestTracker::Padding;
	float Y = QuestTracker::Padding;
	for (const FTrackerEntry& Entry : Entries)
	{
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Entry.Title, TitleFont, FVector2f(Left, Y),
			HudStyle.TextColor, EHudTextAlign::Left);
		if (!Entry.Timer.IsEmpty())
		{
			FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Entry.Timer, TitleFont, FVector2f(Right, Y),
				Entry.TimerColor, EHudTextAlign::Right);
		}
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Entry.Line, LineFont,
			FVector2f(Left, Y + TitleHeight + QuestTracker::LineGap), Entry.LineColor, EHudTextAlign::Left);
		Y += EntryHeight + QuestTracker::EntryGap;
	}

	return TextLayer;
}

#undef LOCTEXT_NAMESPACE
