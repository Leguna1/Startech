#include "ActionBar/ActionBarComponent.h"

#include "ActionBar/ActionBarLayout.h"
#include "ActionBar/ActionSlotProvider.h"
#include "ActionBar/ActionSlotProviderFactory.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"

#if !UE_BUILD_SHIPPING
namespace ActionBarDebug
{
	/** Test aid: fakes a cooldown on the last slot and an active effect on the one before, to see them drawn. */
	static TAutoConsoleVariable<bool> CVarFakeTimers(TEXT("Startech.Test.ActionBarFakeTimers"), false,
		TEXT("Test aid: the last action bar slot shows a cooldown, the one before an active effect (nothing sets them yet)."));
}
#endif

UActionBarComponent::UActionBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

const UActionBarLayout* UActionBarComponent::GetLayout() const
{
	if (!Layout)
	{
		Layout = UStartechSettings::Get().ActionBarLayout.LoadSynchronous();
		if (!Layout)
		{
			UE_LOG(LogTemp, Warning, TEXT("UStartechSettings::ActionBarLayout is not set or missing: the action bar is empty."));
		}
	}
	return Layout;
}

int32 UActionBarComponent::GetSlotCount() const
{
	const UActionBarLayout* const BarLayout = GetLayout();
	return BarLayout ? BarLayout->Slots.Num() : 0;
}

const FActionBarSlot& UActionBarComponent::GetSlot(const int32 Index) const
{
	return GetLayout()->Slots[Index];
}

void UActionBarComponent::BeginPlay()
{
	Super::BeginPlay();

	AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(GetOwner());
	const UActionBarLayout* const BarLayout = GetLayout();
	if (!Ship || !BarLayout)
	{
		return;
	}

	for (const FActionBarSlot& Slot : BarLayout->Slots)
	{
		if (FindProvider(Slot.Kind))
		{
			continue;
		}
		if (UActionSlotProvider* const Provider = FActionSlotProviderFactory::Create(Slot.Kind, *this))
		{
			Provider->Bind(*Ship);
			Provider->OnStateChanged.AddUObject(this, &UActionBarComponent::OnSlotsChangedRelay);
			Providers.Add(Provider);
			ProviderKinds.Add(Slot.Kind);
		}
	}
	OnSlotsChanged.Broadcast();
}

void UActionBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (UActionSlotProvider* const Provider : Providers)
	{
		if (Provider)
		{
			Provider->OnStateChanged.RemoveAll(this);
			Provider->Unbind();
		}
	}
	Providers.Reset();
	ProviderKinds.Reset();
	Super::EndPlay(EndPlayReason);
}

void UActionBarComponent::OnSlotsChangedRelay()
{
	OnSlotsChanged.Broadcast();
}

void UActionBarComponent::SetupInput(UEnhancedInputComponent& Input)
{
	const UActionBarLayout* const BarLayout = GetLayout();
	if (!BarLayout)
	{
		return;
	}
	for (int32 Index = 0; Index < BarLayout->Slots.Num(); ++Index)
	{
		if (const UInputAction* const Action = BarLayout->Slots[Index].InputAction)
		{
			Input.BindAction(Action, ETriggerEvent::Started, this, &UActionBarComponent::HandleSlotPressed, Index);
		}
	}
}

FActionSlotState UActionBarComponent::GetSlotState(const int32 Index, const int32 LowThreshold) const
{
	if (Index < 0 || Index >= GetSlotCount())
	{
		return FActionSlotState();
	}
	const FActionBarSlot& Slot = GetSlot(Index);
	const UActionSlotProvider* const Provider = FindProvider(Slot.Kind);
	FActionSlotState State = Provider ? Provider->GetState(Slot, LowThreshold) : FActionSlotState();

#if !UE_BUILD_SHIPPING
	if (ActionBarDebug::CVarFakeTimers.GetValueOnGameThread())
	{
		// A 6 s cooldown and a 5 s effect, looping on world time.
		const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (Index == GetSlotCount() - 1)
		{
			State.CooldownSecondsLeft = 6.f - FMath::Fmod(Time, 6.f);
			State.CooldownFraction = State.CooldownSecondsLeft / 6.f;
		}
		else if (Index == GetSlotCount() - 2)
		{
			State.ActiveFraction = 1.f - FMath::Fmod(Time, 5.f) / 5.f;
		}
	}
#endif
	return State;
}

FActionSlotVisual UActionBarComponent::GetSlotVisual(const int32 Index) const
{
	if (Index < 0 || Index >= GetSlotCount())
	{
		return FActionSlotVisual();
	}
	const FActionBarSlot& Slot = GetSlot(Index);
	const UActionSlotProvider* const Provider = FindProvider(Slot.Kind);
	return Provider ? Provider->GetVisual(Slot) : FActionSlotVisual();
}

FText UActionBarComponent::GetSlotKeyText(const int32 Index) const
{
	if (Index < 0 || Index >= GetSlotCount() || !GetSlot(Index).InputAction)
	{
		return FText::GetEmpty();
	}
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	const APlayerController* const PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* const Input = PC
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	const TArray<FKey> Keys = Input ? Input->QueryKeysMappedToAction(GetSlot(Index).InputAction) : TArray<FKey>();
	return Keys.IsEmpty() ? FText::GetEmpty() : Keys[0].GetDisplayName(false);
}

bool UActionBarComponent::ActivateSlot(const int32 Index)
{
	if (IsInputBlocked() || Index < 0 || Index >= GetSlotCount())
	{
		return false;
	}
	const FActionBarSlot& Slot = GetSlot(Index);
	UActionSlotProvider* const Provider = FindProvider(Slot.Kind);
	return Provider && Provider->Activate(Slot);
}

void UActionBarComponent::CycleActive(const int32 Direction)
{
	const UActionBarLayout* const BarLayout = GetLayout();
	if (IsInputBlocked() || !BarLayout)
	{
		return;
	}
	for (UActionSlotProvider* const Provider : Providers)
	{
		if (Provider && Provider->Cycle(BarLayout->Slots, Direction))
		{
			return;
		}
	}
}

void UActionBarComponent::HandleSlotPressed(const int32 SlotIndex)
{
	ActivateSlot(SlotIndex);
}

UActionSlotProvider* UActionBarComponent::FindProvider(const EActionSlotKind Kind) const
{
	const int32 Index = ProviderKinds.IndexOfByKey(Kind);
	return Index != INDEX_NONE ? Providers[Index].Get() : nullptr;
}

bool UActionBarComponent::IsInputBlocked() const
{
	const AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(GetOwner());
	return !Ship || Ship->IsHubOpen() || UGameplayStatics::IsGamePaused(this);
}
