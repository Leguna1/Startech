#include "HomeRespawnSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gates/WarpTransitionComponent.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "Station.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechRespawn, Log, All);

namespace HomeRespawn
{
	/** Seconds between checks while waiting for a regeneration or a warp to finish. */
	constexpr float WaitPollSeconds = 0.1f;
}

void UHomeRespawnSubsystem::BeginDeathSequence(AStartechShipPawn& InShip, const bool bCargoDropped,
	const double CargoExpiryTime)
{
	if (IsRespawning())
	{
		UE_LOG(LogStartechRespawn, Warning, TEXT("%s died while a respawn was already running; ignored."),
			*InShip.GetName());
		return;
	}

	const USectorDirectorSubsystem* const Director = GetDirector();
	Ship = &InShip;
	State = EHomeRespawnState::Dying;
	DeathSectorId = Director ? Director->GetCurrentSectorId() : NAME_None;
	bRegenerationRequested = false;
	bDeathCargo = bCargoDropped;
	DeathCargoExpiryTime = CargoExpiryTime;

	BindEvents();
	FadeTo(1.f, UStartechSettings::Get().DeathFadeOutSeconds);

	UE_LOG(LogStartechRespawn, Log, TEXT("Player died in '%s'; home is '%s'."), *DeathSectorId.ToString(),
		*UStartechSettings::Get().HomeSectorId.ToString());
	OnRespawnSequenceChanged.Broadcast(true);
}

void UHomeRespawnSubsystem::RespawnPlayer(AStartechShipPawn& InShip)
{
	if (State != EHomeRespawnState::Dying || Ship.Get() != &InShip)
	{
		// No sequence was started for this ship: respawn it in place, as before.
		InShip.RespawnAt(FindSpawnTransform(InShip));
		return;
	}

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimeoutTimer, this, &UHomeRespawnSubsystem::HandleTimeout,
			UStartechSettings::Get().RespawnTimeoutSeconds, false);
	}
	TryReturnHome();
}

void UHomeRespawnSubsystem::TryReturnHome()
{
	const AStartechShipPawn* const Player = Ship.Get();
	UWorld* const World = GetWorld();
	if (!Player || !World)
	{
		UnbindEvents();
		State = EHomeRespawnState::Idle;
		OnRespawnSequenceChanged.Broadcast(false);
		return;
	}

	USectorDirectorSubsystem* const Director = GetDirector();
	const UWarpTransitionComponent* const Warp = BoundWarp.Get();
	if (Director && (Director->IsBusy() || (Warp && Warp->IsWarping())))
	{
		// Death during a warp or a regeneration: let it end, then go home.
		State = EHomeRespawnState::WaitingForDirector;
		if (!World->GetTimerManager().IsTimerActive(WaitTimer))
		{
			World->GetTimerManager().SetTimer(WaitTimer, this, &UHomeRespawnSubsystem::HandleWaitPoll,
				HomeRespawn::WaitPollSeconds, true);
		}
		return;
	}
	World->GetTimerManager().ClearTimer(WaitTimer);

	const FName HomeSectorId = UStartechSettings::Get().HomeSectorId;
	const FName CurrentSectorId = Director ? Director->GetCurrentSectorId() : NAME_None;
	if (!Director || CurrentSectorId.IsNone() || CurrentSectorId == HomeSectorId)
	{
		FinishRespawn(FindSpawnTransform(*Player));
		return;
	}

	if (!Director->RequestSector(HomeSectorId))
	{
		UE_LOG(LogStartechRespawn, Error,
			TEXT("Home sector '%s' was refused by the sector director (not in the catalog?); respawning in '%s'."),
			*HomeSectorId.ToString(), *CurrentSectorId.ToString());
		FinishRespawn(FindSpawnTransform(*Player));
		return;
	}

	bRegenerationRequested = true;
	State = EHomeRespawnState::Regenerating;
	UE_LOG(LogStartechRespawn, Log, TEXT("Regenerating the level as home sector '%s'."), *HomeSectorId.ToString());
}

void UHomeRespawnSubsystem::HandleWaitPoll()
{
	if (State == EHomeRespawnState::WaitingForDirector)
	{
		TryReturnHome();
	}
}

void UHomeRespawnSubsystem::HandleSectorReady(const FName SectorId)
{
	if (State == EHomeRespawnState::WaitingForDirector)
	{
		// The warp may still hold the screen for a moment; the poll picks that up.
		TryReturnHome();
	}
	else if (State == EHomeRespawnState::Regenerating && SectorId == UStartechSettings::Get().HomeSectorId)
	{
		if (const AStartechShipPawn* const Player = Ship.Get())
		{
			FinishRespawn(FindSpawnTransform(*Player));
		}
	}
}

void UHomeRespawnSubsystem::HandleWarpFinished(const FName SectorId)
{
	if (!IsRespawning())
	{
		return;
	}

	// The warp has just faded in and unlocked input on a dead ship: cover the screen and lock it again.
	FadeTo(1.f, 0.f);
	LockShipInput();

	if (State == EHomeRespawnState::WaitingForDirector)
	{
		TryReturnHome();
	}
}

void UHomeRespawnSubsystem::HandleTimeout()
{
	if (State == EHomeRespawnState::Idle)
	{
		return;
	}

	UE_LOG(LogStartechRespawn, Error,
		TEXT("Respawn timed out after %.1f s (state %d, director busy: %s); respawning at the world origin."),
		UStartechSettings::Get().RespawnTimeoutSeconds, static_cast<int32>(State),
		GetDirector() && GetDirector()->IsBusy() ? TEXT("yes") : TEXT("no"));
	FinishRespawn(FTransform::Identity);
}

void UHomeRespawnSubsystem::FinishRespawn(const FTransform& Spawn)
{
	UnbindEvents();
	State = EHomeRespawnState::Idle;

	if (AStartechShipPawn* const Player = Ship.Get())
	{
		Player->RespawnAt(Spawn);
	}
	FadeTo(0.f, UStartechSettings::Get().RespawnFadeInSeconds);

	OnRespawnSequenceChanged.Broadcast(false);
}

FTransform UHomeRespawnSubsystem::FindSpawnTransform(const AStartechShipPawn& Player) const
{
	const UWorld* const World = GetWorld();
	if (const AStation* const Station = World ? AStation::FindNearestInPlay(*World, Player.GetActorLocation()) : nullptr)
	{
		return Station->GetRespawnTransform();
	}

	const USectorDirectorSubsystem* const Director = GetDirector();
	UE_LOG(LogStartechRespawn, Warning, TEXT("No station in sector '%s'; respawning where play began."),
		Director ? *Director->GetCurrentSectorId().ToString() : TEXT("None"));
	return Player.GetStartTransform();
}

void UHomeRespawnSubsystem::FadeTo(const float ToAlpha, const float Seconds) const
{
	const APlayerController* const PC = GetShipController();
	if (APlayerCameraManager* const Camera = PC ? PC->PlayerCameraManager.Get() : nullptr)
	{
		if (Seconds <= 0.f)
		{
			// A zero-length StartCameraFade never updates FadeAmount; this sets it at once and stops any fade running.
			Camera->SetManualCameraFade(ToAlpha, FLinearColor::Black, false);
			return;
		}

		// From where it is now, so a fade already running (a warp's) doesn't jump.
		Camera->StartCameraFade(Camera->FadeAmount, ToAlpha, Seconds, FLinearColor::Black, false, true);
	}
}

void UHomeRespawnSubsystem::LockShipInput() const
{
	AStartechShipPawn* const Player = Ship.Get();
	if (APlayerController* const PC = GetShipController(); Player && PC)
	{
		Player->DisableInput(PC);
	}
}

void UHomeRespawnSubsystem::BindEvents()
{
	UnbindEvents();

	if (USectorDirectorSubsystem* const Director = GetDirector())
	{
		Director->OnSectorReady.AddUniqueDynamic(this, &UHomeRespawnSubsystem::HandleSectorReady);
		BoundDirector = Director;
	}

	const AStartechShipPawn* const Player = Ship.Get();
	if (UWarpTransitionComponent* const Warp = Player ? Player->FindComponentByClass<UWarpTransitionComponent>() : nullptr)
	{
		Warp->OnWarpFinished.AddUniqueDynamic(this, &UHomeRespawnSubsystem::HandleWarpFinished);
		BoundWarp = Warp;
	}
}

void UHomeRespawnSubsystem::UnbindEvents()
{
	if (USectorDirectorSubsystem* const Director = BoundDirector.Get())
	{
		Director->OnSectorReady.RemoveDynamic(this, &UHomeRespawnSubsystem::HandleSectorReady);
	}
	BoundDirector.Reset();

	if (UWarpTransitionComponent* const Warp = BoundWarp.Get())
	{
		Warp->OnWarpFinished.RemoveDynamic(this, &UHomeRespawnSubsystem::HandleWarpFinished);
	}
	BoundWarp.Reset();

	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitTimer);
		World->GetTimerManager().ClearTimer(TimeoutTimer);
	}
}

void UHomeRespawnSubsystem::Deinitialize()
{
	UnbindEvents();
	State = EHomeRespawnState::Idle;
	Ship.Reset();

	Super::Deinitialize();
}

bool UHomeRespawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UHomeRespawnSubsystem::IsReturningHome() const
{
	return IsRespawning() && (bRegenerationRequested
		|| (!DeathSectorId.IsNone() && DeathSectorId != UStartechSettings::Get().HomeSectorId));
}

FText UHomeRespawnSubsystem::GetHomeDisplayName() const
{
	return GetSectorDisplayName(UStartechSettings::Get().HomeSectorId);
}

FText UHomeRespawnSubsystem::GetDeathSectorDisplayName() const
{
	return GetSectorDisplayName(DeathSectorId);
}

float UHomeRespawnSubsystem::GetDeathCargoSecondsRemaining() const
{
	if (DeathCargoExpiryTime < 0.0)
	{
		return -1.f;
	}

	const UWorld* const World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	return static_cast<float>(FMath::Max(DeathCargoExpiryTime - Now, 0.0));
}

USectorDirectorSubsystem* UHomeRespawnSubsystem::GetDirector() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
}

APlayerController* UHomeRespawnSubsystem::GetShipController() const
{
	const AStartechShipPawn* const Player = Ship.Get();
	return Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
}

FText UHomeRespawnSubsystem::GetSectorDisplayName(const FName SectorId) const
{
	if (SectorId.IsNone())
	{
		return FText::GetEmpty();
	}

	const USectorDirectorSubsystem* const Director = GetDirector();
	const USectorCatalog* const Catalog = Director ? Director->GetCatalog() : nullptr;
	const FSectorDefinition* const Definition = Catalog ? Catalog->FindSector(SectorId) : nullptr;
	return Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(SectorId);
}
