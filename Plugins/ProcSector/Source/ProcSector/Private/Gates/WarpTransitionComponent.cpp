#include "Gates/WarpTransitionComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ProcSectorLog.h"
#include "TimerManager.h"

UWarpTransitionComponent::UWarpTransitionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWarpTransitionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (USectorDirectorSubsystem* Director = World->GetSubsystem<USectorDirectorSubsystem>())
		{
			Director->OnSectorReady.AddDynamic(this, &UWarpTransitionComponent::HandleSectorReady);
		}
	}
}

void UWarpTransitionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
		World->GetTimerManager().ClearTimer(RetryTimerHandle);

		if (USectorDirectorSubsystem* Director = World->GetSubsystem<USectorDirectorSubsystem>())
		{
			Director->OnSectorReady.RemoveDynamic(this, &UWarpTransitionComponent::HandleSectorReady);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool UWarpTransitionComponent::BeginWarp(FName DestinationSectorId)
{
	bool bRefusedByGame = false;
	return TryBeginWarp(DestinationSectorId, true, bRefusedByGame);
}

bool UWarpTransitionComponent::BeginWarpFromTrigger(FName DestinationSectorId, AActor* Trigger)
{
	bool bRefusedByGame = false;
	if (TryBeginWarp(DestinationSectorId, true, bRefusedByGame))
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (bRefusedByGame && Trigger != nullptr && World != nullptr)
	{
		RetryTrigger = Trigger;
		RetrySectorId = DestinationSectorId;
		World->GetTimerManager().SetTimer(RetryTimerHandle, this, &UWarpTransitionComponent::HandleRetryTimer,
			FMath::Max(RefusedRetrySeconds, 0.05f), true);
	}
	return false;
}

void UWarpTransitionComponent::CancelWarpRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimerHandle);
	}
	RetryTrigger.Reset();
	RetrySectorId = NAME_None;
}

bool UWarpTransitionComponent::IsRetryingWarp() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetTimerManager().IsTimerActive(RetryTimerHandle);
}

void UWarpTransitionComponent::HandleRetryTimer()
{
	const AActor* Trigger = RetryTrigger.Get();
	const AActor* Owner = GetOwner();

	// Left the trigger (or the trigger was pooled away, which turns its collision off): no more retries.
	if (State != EWarpState::Idle || Trigger == nullptr || Owner == nullptr || !Trigger->IsOverlappingActor(Owner))
	{
		CancelWarpRetry();
		return;
	}

	bool bRefusedByGame = false;
	if (TryBeginWarp(RetrySectorId, false, bRefusedByGame))
	{
		CancelWarpRetry();
	}
}

bool UWarpTransitionComponent::TryBeginWarp(const FName DestinationSectorId, const bool bBroadcastRefusal,
	bool& bOutRefusedByGame)
{
	bOutRefusedByGame = false;

	UWorld* World = GetWorld();
	USectorDirectorSubsystem* Director = World != nullptr ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;

	if (State != EWarpState::Idle || Director == nullptr || Director->IsBusy())
	{
		return false;
	}

	const USectorCatalog* Catalog = Director->GetCatalog();
	if (Catalog == nullptr || !Catalog->ContainsSector(DestinationSectorId))
	{
		UE_LOG(LogProcSector, Warning, TEXT("Warp refused: unknown sector '%s'."), *DestinationSectorId.ToString());
		return false;
	}

	// The game's own rule last, so its refusal is only reported for a warp that could otherwise start.
	FText Reason;
	if (CanBeginWarp.IsBound() && !CanBeginWarp.Execute(DestinationSectorId, Reason))
	{
		bOutRefusedByGame = true;
		if (bBroadcastRefusal)
		{
			OnWarpRefused.Broadcast(Reason);
		}
		return false;
	}

	// A warp starting by any route ends a pending retry.
	CancelWarpRetry();

	State = EWarpState::FadingOut;
	PendingSectorId = DestinationSectorId;

	SetInputLocked(true);
	StartFade(0.0f, 1.0f, FadeOutSeconds);
	OnWarpStarted.Broadcast(DestinationSectorId);

	World->GetTimerManager().SetTimer(TimerHandle, this, &UWarpTransitionComponent::HandleFadeOutFinished, FMath::Max(FadeOutSeconds, 0.05f), false);
	return true;
}

bool UWarpTransitionComponent::IsWarping() const
{
	return State != EWarpState::Idle;
}

EWarpState UWarpTransitionComponent::GetWarpState() const
{
	return State;
}

void UWarpTransitionComponent::HandleFadeOutFinished()
{
	UWorld* World = GetWorld();
	USectorDirectorSubsystem* Director = World != nullptr ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;

	State = EWarpState::Generating;
	HoldStartSeconds = World != nullptr ? World->GetTimeSeconds() : 0.0;

	if (Director == nullptr || !Director->RequestSector(PendingSectorId))
	{
		// The request was refused; return control in the sector we never left.
		UE_LOG(LogProcSector, Warning, TEXT("Warp aborted: the director refused sector '%s'."), *PendingSectorId.ToString());
		State = EWarpState::Idle;
		StartFade(1.0f, 0.0f, FadeInSeconds);
		SetInputLocked(false);
	}
}

void UWarpTransitionComponent::HandleSectorReady(FName SectorId)
{
	if (State != EWarpState::Generating || SectorId != PendingSectorId)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const double Held = World->GetTimeSeconds() - HoldStartSeconds;
	const float Remaining = MinimumHoldSeconds - static_cast<float>(Held);

	if (Remaining > 0.01f)
	{
		World->GetTimerManager().SetTimer(TimerHandle, this, &UWarpTransitionComponent::CompleteArrival, Remaining, false);
	}
	else
	{
		CompleteArrival();
	}
}

void UWarpTransitionComponent::CompleteArrival()
{
	UWorld* World = GetWorld();
	USectorDirectorSubsystem* Director = World != nullptr ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	APawn* Pawn = GetOwnerPawn();

	FTransform Arrival;
	if (Pawn != nullptr && Director != nullptr && Director->GetArrivalTransform(Director->GetPreviousSectorId(), Arrival))
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}

		Pawn->SetActorLocationAndRotation(Arrival.GetLocation(), Arrival.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);

		if (APlayerController* PlayerController = GetOwnerPlayerController())
		{
			PlayerController->SetControlRotation(Arrival.Rotator());
		}
	}

	State = EWarpState::Idle;
	StartFade(1.0f, 0.0f, FadeInSeconds);
	SetInputLocked(false);
	OnWarpFinished.Broadcast(PendingSectorId);
}

void UWarpTransitionComponent::StartFade(const float FromAlpha, const float ToAlpha, const float Duration) const
{
	if (const APlayerController* PlayerController = GetOwnerPlayerController())
	{
		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			// Hold the final colour so the screen stays covered between fade-out and fade-in.
			CameraManager->StartCameraFade(FromAlpha, ToAlpha, Duration, FadeColor, false, true);
		}
	}
}

void UWarpTransitionComponent::SetInputLocked(const bool bLocked) const
{
	APawn* Pawn = GetOwnerPawn();
	APlayerController* PlayerController = GetOwnerPlayerController();
	if (Pawn == nullptr || PlayerController == nullptr)
	{
		return;
	}

	if (bLocked)
	{
		Pawn->DisableInput(PlayerController);
	}
	else
	{
		Pawn->EnableInput(PlayerController);
	}
}

APawn* UWarpTransitionComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

APlayerController* UWarpTransitionComponent::GetOwnerPlayerController() const
{
	const APawn* Pawn = GetOwnerPawn();
	return Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}
