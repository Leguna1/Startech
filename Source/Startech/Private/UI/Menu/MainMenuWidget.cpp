#include "UI/Menu/MainMenuWidget.h"

#include "Components/WidgetSwitcher.h"
#include "Flow/StartechGameFlowSubsystem.h"
#include "UI/Controls/ControlsScreenWidget.h"
#include "UI/Settings/SettingsScreenWidget.h"
#include "UI/Menu/MainMenuCallsignPanel.h"
#include "UI/Menu/MainMenuDialog.h"
#include "UI/Menu/MainMenuHomePanel.h"
#include "UI/Menu/MainMenuSlotCard.h"
#include "UI/Menu/MainMenuSlotsPanel.h"

#define LOCTEXT_NAMESPACE "MainMenu"

UMainMenuWidget::UMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!ControlsScreen)
	{
		ControlsScreen = UMenuPageWidget::AddToMenu<UControlsScreenWidget>(*this, TEXT("ControlsScreen"));
	}
	if (!SettingsScreen)
	{
		SettingsScreen = UMenuPageWidget::AddToMenu<USettingsScreenWidget>(*this, TEXT("SettingsScreen"));
		if (SettingsScreen)
		{
			SettingsScreen->SetShowMenuMusic(true);
		}
	}
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HomePanel->OnContinue.AddUObject(this, &UMainMenuWidget::HandleContinue);
	HomePanel->OnNewGame.AddUObject(this, &UMainMenuWidget::ShowSlots, false);
	HomePanel->OnLoad.AddUObject(this, &UMainMenuWidget::ShowSlots, true);
	HomePanel->OnControls.AddUObject(this, &UMainMenuWidget::OpenControls);
	HomePanel->OnSettings.AddUObject(this, &UMainMenuWidget::OpenSettings);
	HomePanel->OnQuit.AddLambda([this]()
	{
		if (UStartechGameFlowSubsystem* const Flow = GetFlow())
		{
			Flow->QuitGame();
		}
	});

	SlotsPanel->OnSlotPicked.AddUObject(this, &UMainMenuWidget::HandleSlotPicked);
	SlotsPanel->OnDeleteRequested.AddUObject(this, &UMainMenuWidget::HandleDeleteRequested);
	SlotsPanel->OnBack.AddUObject(this, &UMainMenuWidget::ShowHome);

	CallsignPanel->OnStart.AddUObject(this, &UMainMenuWidget::HandleStart);
	CallsignPanel->OnBack.AddUObject(this, &UMainMenuWidget::ShowSlots, false);

	if (ControlsScreen)
	{
		ControlsScreen->OnClosed.AddUObject(this, &UMainMenuWidget::HandlePageClosed);
	}
	if (SettingsScreen)
	{
		SettingsScreen->OnClosed.AddUObject(this, &UMainMenuWidget::HandlePageClosed);
	}

	ShowHome();
}

void UMainMenuWidget::NativeDestruct()
{
	HomePanel->OnContinue.RemoveAll(this);
	HomePanel->OnNewGame.RemoveAll(this);
	HomePanel->OnLoad.RemoveAll(this);
	HomePanel->OnControls.RemoveAll(this);
	HomePanel->OnSettings.RemoveAll(this);
	HomePanel->OnQuit.Clear();
	SlotsPanel->OnSlotPicked.RemoveAll(this);
	SlotsPanel->OnDeleteRequested.RemoveAll(this);
	SlotsPanel->OnBack.RemoveAll(this);
	CallsignPanel->OnStart.RemoveAll(this);
	CallsignPanel->OnBack.RemoveAll(this);
	if (ControlsScreen)
	{
		ControlsScreen->OnClosed.RemoveAll(this);
	}
	if (SettingsScreen)
	{
		SettingsScreen->OnClosed.RemoveAll(this);
	}
	Super::NativeDestruct();
}

FReply UMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Escape || InKeyEvent.IsRepeat())
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	if (UMenuPageWidget* const Page = GetOpenPage())
	{
		Page->Close();
	}
	else if (Dialog->IsOpen())
	{
		Dialog->Cancel();
	}
	else if (PageSwitcher->GetActiveWidget() == CallsignPanel)
	{
		ShowSlots(false);
	}
	else if (PageSwitcher->GetActiveWidget() == SlotsPanel)
	{
		ShowHome();
	}
	return FReply::Handled();
}

void UMainMenuWidget::OpenControls()
{
	OpenPage(ControlsScreen);
}

void UMainMenuWidget::OpenSettings()
{
	OpenPage(SettingsScreen);
}

void UMainMenuWidget::OpenPage(UMenuPageWidget* const Page)
{
	if (!Page || GetOpenPage())
	{
		return;
	}
	VisibilityBeforePage = PageSwitcher->GetVisibility();
	PageSwitcher->SetVisibility(ESlateVisibility::Collapsed);
	Page->Open();
	SetKeyboardFocus();
}

void UMainMenuWidget::HandlePageClosed()
{
	PageSwitcher->SetVisibility(VisibilityBeforePage);
	ShowHome();
}

UMenuPageWidget* UMainMenuWidget::GetOpenPage() const
{
	if (ControlsScreen && ControlsScreen->IsOpen())
	{
		return ControlsScreen;
	}
	return SettingsScreen && SettingsScreen->IsOpen() ? SettingsScreen.Get() : nullptr;
}

UHubButtonWidget* UMainMenuWidget::GetSettingsButton() const
{
	return HomePanel ? HomePanel->GetSettingsButton() : nullptr;
}

UHubButtonWidget* UMainMenuWidget::GetControlsButton() const
{
	return HomePanel ? HomePanel->GetControlsButton() : nullptr;
}

void UMainMenuWidget::ShowHome()
{
	const UStartechGameFlowSubsystem* const Flow = GetFlow();
	const int32 Recent = Flow ? Flow->FindMostRecentSlot() : INDEX_NONE;

	FText Detail;
	if (Recent != INDEX_NONE)
	{
		for (const FSaveSlotInfo& Info : Flow->GetVisibleSlotInfos())
		{
			if (Info.Header.SlotIndex == Recent)
			{
				Detail = FText::Format(LOCTEXT("ContinueDetail", "{0}  ·  {1} played"), FText::FromString(Info.Header.DisplayName),
					UMainMenuSlotCard::FormatPlayTime(Info.Header.PlayTimeSeconds));
			}
		}
	}
	HomePanel->SetContinue(Recent != INDEX_NONE, Detail);
	HomePanel->SetLoadEnabled(Recent != INDEX_NONE);
	PageSwitcher->SetActiveWidget(HomePanel);
	SetKeyboardFocus();
}

void UMainMenuWidget::ShowSlots(const bool bLoadMode)
{
	const UStartechGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow)
	{
		return;
	}
	if (bLoadMode && !Flow->HasAnySave())
	{
		ShowHome();
		return;
	}
	bSlotsLoadMode = bLoadMode;
	SlotsPanel->Show(bLoadMode ? EMainMenuSlotMode::Load : EMainMenuSlotMode::NewGame, Flow->GetVisibleSlotInfos());
	PageSwitcher->SetActiveWidget(SlotsPanel);
	SetKeyboardFocus();
}

void UMainMenuWidget::ShowCallsign(const int32 SlotIndex)
{
	PendingNewGameSlot = SlotIndex;
	CallsignPanel->Prepare(SlotIndex);
	PageSwitcher->SetActiveWidget(CallsignPanel);
}

void UMainMenuWidget::HandleContinue()
{
	FText Reason;
	UStartechGameFlowSubsystem* const Flow = GetFlow();
	if (Flow && !Flow->ContinueGame(Reason))
	{
		Dialog->ShowMessage(FText::Format(LOCTEXT("ContinueFailed", "The game could not be loaded. {0}"), Reason));
		ShowHome();
	}
}

void UMainMenuWidget::HandleSlotPicked(const int32 SlotIndex)
{
	UStartechGameFlowSubsystem* const Flow = GetFlow();
	if (!Flow)
	{
		return;
	}

	if (bSlotsLoadMode)
	{
		FText Reason;
		if (!Flow->LoadGame(SlotIndex, Reason))
		{
			SlotsPanel->SetStatus(FText::Format(LOCTEXT("LoadFailed", "{0} could not be loaded. {1}"), GetSlotName(SlotIndex), Reason));
		}
		return;
	}

	const TArray<FSaveSlotInfo> Infos = Flow->GetVisibleSlotInfos();
	const FSaveSlotInfo* const Info = Infos.FindByPredicate([SlotIndex](const FSaveSlotInfo& Each) { return Each.Header.SlotIndex == SlotIndex; });
	if (Info && Info->bOccupied)
	{
		Dialog->Ask(FText::Format(LOCTEXT("Overwrite", "Overwrite {0}? This can't be undone."), GetSlotName(SlotIndex)),
			LOCTEXT("OverwriteButton", "Overwrite"), [this, SlotIndex]() { ShowCallsign(SlotIndex); });
		return;
	}
	ShowCallsign(SlotIndex);
}

void UMainMenuWidget::HandleDeleteRequested(const int32 SlotIndex)
{
	Dialog->Ask(FText::Format(LOCTEXT("Delete", "Delete {0}? This can't be undone."), GetSlotName(SlotIndex)),
		LOCTEXT("DeleteButton", "Delete"), [this, SlotIndex]()
		{
			if (UStartechGameFlowSubsystem* const Flow = GetFlow())
			{
				Flow->DeleteSlot(SlotIndex);
			}
			// Back home once the last save is gone; otherwise the shorter list.
			ShowSlots(true);
		});
}

void UMainMenuWidget::HandleStart(const FString& Callsign)
{
	FText Reason;
	UStartechGameFlowSubsystem* const Flow = GetFlow();
	if (Flow && !Flow->StartNewGame(PendingNewGameSlot, Callsign, Reason))
	{
		Dialog->ShowMessage(Reason);
	}
}

FText UMainMenuWidget::GetSlotName(const int32 SlotIndex) const
{
	if (const UStartechGameFlowSubsystem* const Flow = GetFlow())
	{
		for (const FSaveSlotInfo& Info : Flow->GetVisibleSlotInfos())
		{
			if (Info.Header.SlotIndex == SlotIndex && Info.bOccupied && !Info.Header.DisplayName.IsEmpty())
			{
				return FText::FromString(Info.Header.DisplayName);
			}
		}
	}
	return FText::Format(LOCTEXT("SlotN", "Slot {0}"), FText::AsNumber(SlotIndex + 1));
}

UStartechGameFlowSubsystem* UMainMenuWidget::GetFlow() const
{
	return IsDesignTime() ? nullptr : UStartechGameFlowSubsystem::Get(this);
}

#undef LOCTEXT_NAMESPACE
