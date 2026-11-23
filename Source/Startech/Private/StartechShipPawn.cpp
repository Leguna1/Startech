#include "StartechShipPawn.h"

#include "Audio/StartechAudioSubsystem.h"
#include "Audio/StartechMusicSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Death/ShipDeathComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Gates/WarpTransitionComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "HomeRespawnSubsystem.h"
#include "GameFramework/DamageType.h"
#include "InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LootPickup.h"
#include "MapMarkerComponent.h"
#include "SectorBounds.h"
#include "DespawnSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "ActionBar/ActionBarComponent.h"
#include "Boost/ShipBoostComponent.h"
#include "Upgrades/ShipUpgradeStatsComponent.h"
#include "Flow/StartechInputState.h"
#include "Flow/StartechPauseMenuSubsystem.h"
#include "InputAction.h"
#include "Runtime/SaveSubsystem.h"
#include "Save/StartechSaveParticipantFactory.h"
#include "UI/Hub/StationHubWidget.h"
#include "Station.h"
#include "StartechQuestBridge.h"
#include "StartechSettings.h"
#include "StartechHUDLog.h"
#include "TargetRangeComponent.h"
#include "TimerManager.h"
#include "TargetableComponent.h"
#include "TargetingComponent.h"
#include "WeaponSystemComponent.h"

namespace StartechShipPawn
{
	/** The station hub draws over the HUD. */
	constexpr int32 HubZOrder = 10;
}

AStartechShipPawn::AStartechShipPawn()
{
	Team = ETeam::Player;

	// ShipMesh (the root) is created by AStartechShipBase.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(ShipMesh);
	CameraBoom->TargetArmLength = 1200.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 300.f);
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 6.f;
	
	CameraBoom->bInheritYaw = false;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Uses the "Cockpit" socket on the mesh if it exists, otherwise the mesh origin.
	CockpitRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CockpitRoot"));
	CockpitRoot->SetupAttachment(ShipMesh, TEXT("Cockpit"));

	// Inherits the ship's full rotation, so the world tilts outside the window.
	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
	CockpitCamera->SetupAttachment(CockpitRoot);
	CockpitCamera->SetAutoActivate(false);
	CockpitCamera->SetFieldOfView(CockpitBaseFOV);

	TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));

	TargetRange = CreateDefaultSubobject<UTargetRangeComponent>(TEXT("TargetRange"));
	ActionBar = CreateDefaultSubobject<UActionBarComponent>(TEXT("ActionBar"));
	Boost = CreateDefaultSubobject<UShipBoostComponent>(TEXT("Boost"));
	UpgradeStats = CreateDefaultSubobject<UShipUpgradeStatsComponent>(TEXT("UpgradeStats"));
}

FLootBundle AStartechShipPawn::CollectLoot(const FLootBundle& Loot)
{
	FLootBundle Taken;
	FLootBundle Remainder;

	if (Loot.Credits > 0)
	{
		Inventory->AddCredits(Loot.Credits);
		Taken.Credits = Loot.Credits;
	}

	for (const FItemStack& Stack : Loot.Items)
	{
		if (!Stack.Item || Stack.Count <= 0)
		{
			continue;
		}

		const int32 Added = Inventory->AddItem(Stack.Item, Stack.Count);
		if (Added > 0)
		{
			FItemStack& TakenStack = Taken.Items.AddDefaulted_GetRef();
			TakenStack.Item = Stack.Item;
			TakenStack.Count = Added;
		}
		if (Added < Stack.Count)
		{
			FItemStack& Left = Remainder.Items.AddDefaulted_GetRef();
			Left.Item = Stack.Item;
			Left.Count = Stack.Count - Added;
		}
	}

	for (const FAmmoStack& Stack : Loot.Ammo)
	{
		if (Stack.Ammo && Stack.Count > 0.f)
		{
			WeaponSystem->AddAmmo(Stack.Ammo, Stack.Count);
			Taken.Ammo.Add(Stack);
		}
	}

	if (!Taken.IsEmpty())
	{
		OnLootCollected.Broadcast(Taken);
	}
	return Remainder;
}

void AStartechShipPawn::BeginPlay()
{
	Super::BeginPlay();

	// The eye position you place in BP_Ship is the rest position for head sway.
	EyeLocation = CockpitCamera->GetRelativeLocation();

	// The boost widens the field of view from here, and adds position lag only while it lasts.
	ChaseBaseFOV = ChaseCamera->FieldOfView;
	CameraBoom->bEnableCameraLag = false;

	// Start in chase view. Done here so interior parts added in BP_Ship get hidden too.
	SetCockpitView(false);

	StartTransform = GetActorTransform();

	HealthComponent->OnDamageTaken.AddUniqueDynamic(this, &AStartechShipPawn::HandleDamageTaken);

	// No warping in combat, and no damage while warping. The component is added in BP_Ship.
	if (UWarpTransitionComponent* const Warp = FindComponentByClass<UWarpTransitionComponent>())
	{
		Warp->CanBeginWarp.BindUObject(this, &AStartechShipPawn::CanWarp);
		Warp->OnWarpStarted.AddUniqueDynamic(this, &AStartechShipPawn::HandleWarpStarted);
		Warp->OnWarpFinished.AddUniqueDynamic(this, &AStartechShipPawn::HandleWarpFinished);
		BoundWarp = Warp;
	}

	if (UWorld* const World = GetWorld())
	{
		for (TActorIterator<ASectorBounds> It(World); It; ++It)
		{
			SectorBounds = *It;
			break;
		}
	}

	// Quests read this ship's cargo, ammo, credits and docking. The ship survives death, so this binds once.
	if (UStartechQuestBridge* const QuestBridge = UStartechQuestBridge::Get(this))
	{
		QuestBridge->BindPlayerShip(*this);
	}

	// After Super::BeginPlay, so the components' Blueprint defaults are in place before a save replaces them.
	if (USaveSubsystem* const Saves = USaveSubsystem::Get(this))
	{
		for (UObject* const Participant : FStartechSaveParticipantFactory::CreateShipParticipants(*this))
		{
			SaveParticipants.Add(Participant);
			Saves->RegisterParticipant(Participant);
		}
	}
}

void AStartechShipPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USaveSubsystem* const Saves = USaveSubsystem::Get(this))
	{
		for (UObject* const Participant : SaveParticipants)
		{
			Saves->UnregisterParticipant(Participant);
		}
	}
	SaveParticipants.Reset();

	Super::EndPlay(EndPlayReason);
}

void AStartechShipPawn::HandleDeath(AActor* const Killer)
{
	// Switches off the targetable, if the ship has one.
	Super::HandleDeath(Killer);

	CloseHub();

	// Stop firing and drop every held intent: once input is off, key-release events never arrive. The velocity
	// stays: the wreck drifts through the dying state.
	ClearIntents();
	ClearFacingLocation();
	Boost->StopImmediately();
	SetFlightScales(GetNormalSpeedScale(), 1.f);
	WeaponSystem->UpdateFiring(false, GetActorLocation(), 0.f);

	// No lock while dead, and no new soft lock picked up while the ship waits.
	TargetingComponent->ClearTarget();
	TargetingComponent->SetComponentTickEnabled(false);

	// The cockpit camera is on the hull and would tumble with it: watch the wreck from the chase camera.
	bCockpitBeforeDeath = bInCockpit;
	if (bInCockpit)
	{
		SetCockpitView(false);
	}

	if (APlayerController* const PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
}

void AStartechShipPawn::HandleDyingFinished(const FVector& Location)
{
	Super::HandleDyingFinished(Location);

	// Already hidden by the explosion. Collision off too, so the dropped cargo isn't scooped straight back up.
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);

	const bool bCargoDropped = DropCargo(GetDeathDropLocation(Location));

	ResetSectorTimer();

	OnPlayerDied.Broadcast();

	// Covers the screen and shows the death message; the same world time as the cargo's own expiry.
	if (UHomeRespawnSubsystem* const HomeRespawn = GetWorld()->GetSubsystem<UHomeRespawnSubsystem>())
	{
		const double CargoExpiryTime = DeathLootLifetime > 0.f ? GetWorld()->GetTimeSeconds() + DeathLootLifetime : -1.0;
		HomeRespawn->BeginDeathSequence(*this, bCargoDropped, CargoExpiryTime);
	}

	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AStartechShipPawn::Respawn,
		FMath::Max(RespawnDelay, KINDA_SMALL_NUMBER));
}

bool AStartechShipPawn::DropCargo(const FVector& Location)
{
	if (!DeathLootPickupClass)
	{
		if (Inventory->GetCargoUsed() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s has no DeathLootPickupClass; no cargo dropped."), *GetName());
		}
		return false;
	}

	const float MinFraction = FMath::Clamp(DeathDropMinFraction, 0.f, 1.f);
	const float MaxFraction = FMath::Clamp(FMath::Max(DeathDropMaxFraction, MinFraction), 0.f, 1.f);

	FLootBundle Dropped;

	// A copy, because removing items changes the inventory's stacks.
	const TArray<FItemStack> Held = Inventory->GetItems();
	for (const FItemStack& Stack : Held)
	{
		const int32 Count = FMath::Min(FMath::RoundToInt(Stack.Count * FMath::FRandRange(MinFraction, MaxFraction)), Stack.Count);
		if (!Stack.Item || Count <= 0 || !Inventory->RemoveItem(Stack.Item, Count))
		{
			continue;
		}

		FItemStack& Lost = Dropped.Items.AddDefaulted_GetRef();
		Lost.Item = Stack.Item;
		Lost.Count = Count;
	}

	// Pooled; the subsystem stores it when the player warps away and puts it back in this sector until it expires.
	UDespawnSubsystem* const Despawner = GetWorld() ? GetWorld()->GetSubsystem<UDespawnSubsystem>() : nullptr;
	return Despawner && Despawner->DropDeathLoot(DeathLootPickupClass, Location, Dropped, DeathLootLifetime) != nullptr;
}

FVector AStartechShipPawn::GetDeathDropLocation(const FVector& DeathLocation) const
{
	// The play area is a box of half-size Extent centred on the level origin.
	const UWorld* const World = GetWorld();
	const USectorDirectorSubsystem* const Director = World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	if (Director && !Director->GetCurrentSectorId().IsNone())
	{
		const FVector Extent = Director->GetPlayAreaExtent();
		const bool bOutside = FMath::Abs(DeathLocation.X) > Extent.X || FMath::Abs(DeathLocation.Y) > Extent.Y
			|| FMath::Abs(DeathLocation.Z) > Extent.Z;
		if (!bOutside)
		{
			return DeathLocation;
		}

		// Well inside, where it can still be reached. A margin wider than an axis puts it on the origin for that axis.
		const FVector Inner = (Extent - FVector(FMath::Max(DeathLootPlayAreaMargin, 0.f))).ComponentMax(FVector::ZeroVector);
		return DeathLocation.BoundToBox(-Inner, Inner);
	}

	// No ProcSector sector (a hand-built level): fall back to the level's bounds box.
	const ASectorBounds* const Bounds = SectorBounds.Get();
	return Bounds && !Bounds->IsInside(DeathLocation) ? Bounds->GetClosestPointInside(DeathLocation) : DeathLocation;
}

void AStartechShipPawn::Respawn()
{
	if (UHomeRespawnSubsystem* const HomeRespawn = GetWorld()->GetSubsystem<UHomeRespawnSubsystem>())
	{
		HomeRespawn->RespawnPlayer(*this);
		return;
	}

	// No subsystem (an editor-preview world): the nearest station in play, else where play began.
	const AStation* const Station = AStation::FindNearestInPlay(*GetWorld(), GetActorLocation());
	RespawnAt(Station ? Station->GetRespawnTransform() : StartTransform);
}

void AStartechShipPawn::RespawnAt(const FTransform& Spawn)
{

	// Yaw only: the flight model keeps pitch and roll as visual lean.
	SetActorLocationAndRotation(Spawn.GetLocation(), FRotator(0.f, Spawn.Rotator().Yaw, 0.f),
		false, nullptr, ETeleportType::TeleportPhysics);
	ResetFlightState();
	LastVelocity = FVector::ZeroVector;

	// Collision and materials as before the death, before collision comes back on below.
	ShipDeath->ResetDeath();

	// The upgrades first, so the revive fills to the upgraded maximums.
	UpgradeStats->ApplyStats();
	HealthComponent->Revive();
	Boost->Refill();
	ResetSectorTimer();
	LastDamageTime = -1.0;

	if (UTargetableComponent* const Targetable = FindComponentByClass<UTargetableComponent>())
	{
		Targetable->SetTargetable(true);
	}

	// Only if the Blueprint gave the player a marker; the base HandleDeath disabled it.
	if (UMapMarkerComponent* const Marker = FindComponentByClass<UMapMarkerComponent>())
	{
		Marker->SetMarkerEnabled(true);
	}

	TargetingComponent->SetComponentTickEnabled(true);

	SetActorHiddenInGame(false);

	if (bCockpitBeforeDeath)
	{
		bCockpitBeforeDeath = false;
		SetCockpitView(true);
	}

	// Back on after the move, so the overlap update docks the ship if it landed in a station's zone.
	SetActorEnableCollision(true);

	if (APlayerController* const PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}

	OnPlayerRespawned.Broadcast();
}

void AStartechShipPawn::PlaceAt(const FTransform& Spawn)
{
	// Collision off for the move and back on after, like RespawnAt, so the station's zone docks the ship at once.
	SetActorEnableCollision(false);
	SetActorLocationAndRotation(Spawn.GetLocation(), FRotator(0.f, Spawn.Rotator().Yaw, 0.f),
		false, nullptr, ETeleportType::TeleportPhysics);
	ResetFlightState();
	LastVelocity = FVector::ZeroVector;
	LookYaw = 0.f;
	LookPitch = 0.f;
	SetActorEnableCollision(true);
}

void AStartechShipPawn::SetDockedStation(AStation* const Station)
{
	const AStation* const OldStation = GetDockedStation();

	Super::SetDockedStation(Station);

	// Undocking (a station pooled away mid-warp, say) must not end the warp's invulnerability.
	if (bWarpProtected)
	{
		HealthComponent->SetInvulnerable(true);
	}

	if (!Station)
	{
		CloseHub();
	}

	if (Station != OldStation)
	{
		const FText StationName = Station ? Station->GetDisplayName() : FText::GetEmpty();
		STARTECH_HUD_LOG(TEXT("OnDockedChanged: %s '%s'"), Station ? TEXT("docked") : TEXT("undocked"),
			*StationName.ToString());
		OnDockedChanged.Broadcast(Station != nullptr, StationName);
	}
}

void AStartechShipPawn::HandleDamageTaken(const float Amount, const bool bHitShield, const FVector SourceLocation)
{
	if (Amount > 0.f)
	{
		LastDamageTime = GetWorld()->GetTimeSeconds();
	}
}

bool AStartechShipPawn::IsInCombat() const
{
	return GetCombatSecondsRemaining() > 0.f;
}

float AStartechShipPawn::GetCombatSecondsRemaining() const
{
	if (LastDamageTime < 0.0)
	{
		return 0.f;
	}

	const double Since = GetWorld()->GetTimeSeconds() - LastDamageTime;
	return static_cast<float>(FMath::Max(UStartechSettings::Get().CombatCooldownSeconds - Since, 0.0));
}

bool AStartechShipPawn::CanWarp(const FName DestinationSectorId, FText& OutReason) const
{
	if (IsInCombat())
	{
		OutReason = NSLOCTEXT("StartechShip", "WarpInCombat", "Can't warp while in combat");
		return false;
	}
	return true;
}

void AStartechShipPawn::HandleWarpStarted(const FName DestinationSectorId)
{
	bWarpProtected = true;
	HealthComponent->SetInvulnerable(true);

	// The music starts fading with the screen, a little before the director's sector change.
	if (const UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		Audio->PlayWarpSound(this);
	}
	if (UStartechMusicSubsystem* const Music = UStartechMusicSubsystem::Get(this))
	{
		Music->PrepareSectorChange(DestinationSectorId);
	}
}

void AStartechShipPawn::HandleWarpFinished(const FName SectorId)
{
	bWarpProtected = false;
	HealthComponent->SetInvulnerable(IsDocked());
}

void AStartechShipPawn::OnInteract()
{
	// While the hub is open, input is UI-only and the hub handles this key itself.
	if (IsDocked())
	{
		OpenHub();
	}
}

void AStartechShipPawn::OpenHub()
{
	APlayerController* const PC = Cast<APlayerController>(GetController());
	if (IsHubOpen() || !IsDocked() || HealthComponent->IsDead() || !PC)
	{
		return;
	}

	if (!StationHubClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no StationHubClass; the station hub can't open."), *GetName());
		return;
	}

	StationHub = CreateWidget<UStationHubWidget>(PC, StationHubClass);
	if (!StationHub)
	{
		return;
	}
	StationHub->AddToViewport(StartechShipPawn::HubZOrder);

	ClearIntents();

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(StationHub->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);

	// Effects fade to silent while the hub is open; the music plays on.
	if (UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		Audio->SetHubOpen(true);
	}
	OnHubOpenChanged.Broadcast(true);
}

void AStartechShipPawn::CloseHub()
{
	if (!IsHubOpen())
	{
		return;
	}

	UStationHubWidget* const Hub = StationHub;
	StationHub = nullptr;
	Hub->RemoveFromParent();

	if (APlayerController* const PC = Cast<APlayerController>(GetController()))
	{
		FStartechInputState::SetGameOnly(*PC);
	}

	if (UStartechAudioSubsystem* const Audio = UStartechAudioSubsystem::Get(this))
	{
		Audio->SetHubOpen(false);
	}
	OnHubOpenChanged.Broadcast(false);
}

void AStartechShipPawn::UndockFromStation()
{
	const AStation* const Station = GetDockedStation();
	if (!Station)
	{
		CloseHub();
		return;
	}

	const FTransform Undock = Station->GetUndockTransform(*this);

	// Yaw only, as on respawn: the flight model keeps pitch and roll as visual lean. Leaving the zone's overlap
	// undocks the ship (SetDockedStation), which also closes the hub.
	SetActorLocationAndRotation(Undock.GetLocation(), FRotator(0.f, Undock.Rotator().Yaw, 0.f),
		false, nullptr, ETeleportType::TeleportPhysics);
	ResetFlightState();
	LastVelocity = FVector::ZeroVector;
	LookYaw = 0.f;
	LookPitch = 0.f;

	CloseHub();
}

bool AStartechShipPawn::IsInteractKey(const FKey& Key) const
{
	const APlayerController* const PC = Cast<APlayerController>(GetController());
	const UEnhancedInputLocalPlayerSubsystem* const Input = PC && InteractAction
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	return Input && Input->QueryKeysMappedToAction(InteractAction).Contains(Key);
}

FText AStartechShipPawn::GetInteractKeyText() const
{
	const APlayerController* const PC = Cast<APlayerController>(GetController());
	const UEnhancedInputLocalPlayerSubsystem* const Input = PC && InteractAction
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	const TArray<FKey> Keys = Input ? Input->QueryKeysMappedToAction(InteractAction) : TArray<FKey>();
	return Keys.IsEmpty() ? FText::GetEmpty() : Keys[0].GetDisplayName(false);
}

void AStartechShipPawn::ClearIntents()
{
	bWantsToFire = false;
	Boost->SetBoostHeld(false);
	RawTurnAxis = 0.f;
	SetThrottleInput(0.f);
	SetVerticalInput(0.f);
	SetTurnInput(0.f);
	SetStrafeInput(0.f);
}

bool AStartechShipPawn::IsTriggerHeld() const
{
	return bWantsToFire && !IsDocked();
}

void AStartechShipPawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	const APlayerController* const PC = Cast<APlayerController>(GetController());
	if (!PC || !MappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* const Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}
}

void AStartechShipPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* const Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	// Completed fires with a zero value, which clears the axis on key release.
	Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ThisClass::OnThrottle);
	Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ThisClass::OnThrottle);
	Input->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ThisClass::OnTurn);
	Input->BindAction(TurnAction, ETriggerEvent::Completed, this, &ThisClass::OnTurn);
	Input->BindAction(VerticalAction, ETriggerEvent::Triggered, this, &ThisClass::OnVertical);
	Input->BindAction(VerticalAction, ETriggerEvent::Completed, this, &ThisClass::OnVertical);
	Input->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ThisClass::OnToggleView);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::OnLook);
	Input->BindAction(LockTargetAction, ETriggerEvent::Started, TargetingComponent.Get(), &UTargetingComponent::ToggleHardLock);
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::OnFireStarted);
	Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ThisClass::OnFireCompleted);
	Input->BindAction(CycleSlotAction, ETriggerEvent::Triggered, this, &ThisClass::OnCycleSlot);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::OnInteract);

	// Keys 1-6 (IA_Slot1...): the action bar layout's slots.
	ActionBar->SetupInput(*Input);

	// Left Shift (IA_Boost): from the settings, like the pause action.
	if (const UInputAction* const BoostAction = UStartechSettings::Get().BoostInputAction.LoadSynchronous())
	{
		Input->BindAction(BoostAction, ETriggerEvent::Started, this, &ThisClass::OnBoostStarted);
		Input->BindAction(BoostAction, ETriggerEvent::Completed, this, &ThisClass::OnBoostCompleted);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UStartechSettings::BoostInputAction is not set or missing: Left Shift won't boost."));
	}

	// From the settings rather than BP_Ship, like the other screens' classes.
	if (const UInputAction* const PauseAction = UStartechSettings::Get().PauseInputAction.LoadSynchronous())
	{
		Input->BindAction(PauseAction, ETriggerEvent::Started, this, &ThisClass::OnPause);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UStartechSettings::PauseInputAction is not set or missing: Escape won't pause."));
	}
}

void AStartechShipPawn::OnPause()
{
	// While the hub is open, input is UI-only and the hub closes on Escape itself.
	if (UStartechPauseMenuSubsystem* const PauseMenus = UStartechPauseMenuSubsystem::Get(this))
	{
		PauseMenus->OpenPauseMenu();
	}
}

void AStartechShipPawn::OnBoostStarted()
{
	if (IsBoostAllowed())
	{
		Boost->SetBoostHeld(true);
	}
}

void AStartechShipPawn::OnBoostCompleted()
{
	Boost->SetBoostHeld(false);
}

bool AStartechShipPawn::IsBoostAllowed() const
{
	if (IsHubOpen() || HealthComponent->IsDead())
	{
		return false;
	}

	const UWarpTransitionComponent* const Warp = BoundWarp.Get();
	if (Warp && Warp->IsWarping())
	{
		return false;
	}

	const UStartechPauseMenuSubsystem* const PauseMenus = UStartechPauseMenuSubsystem::Get(this);
	return !PauseMenus || !PauseMenus->IsPauseMenuOpen();
}

void AStartechShipPawn::UpdateBoost(const float DeltaSeconds)
{
	Boost->UpdateBoost(DeltaSeconds, IsBoostAllowed());
	SetFlightScales(GetNormalSpeedScale(), Boost->GetSpeedMultiplier());

	// Eased, so the camera settles in and out of the boost rather than following the multiplier's straight ramp.
	const FShipBoostSettings& Settings = UStartechSettings::Get().Boost;
	const float Feel = FMath::SmoothStep(0.f, 1.f, Boost->GetBoostAlpha());
	ChaseCamera->SetFieldOfView(ChaseBaseFOV + Settings.FieldOfViewWidening * Feel);

	CameraBoom->bEnableCameraLag = Feel > 0.f;
	CameraBoom->CameraLagSpeed = FMath::Lerp(Settings.CameraLagSpeedAtStart, Settings.CameraLagSpeedAtFullBoost, Feel);
}

// While the station hub is open, flight, look and fire input is ignored: the intents were zeroed on opening.
void AStartechShipPawn::OnFireStarted()   { bWantsToFire = !IsHubOpen(); }
void AStartechShipPawn::OnFireCompleted() { bWantsToFire = false; }


void AStartechShipPawn::OnCycleSlot(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (IsHubOpen() || FMath::IsNearlyZero(Axis))
	{
		return;
	}

	// Positive (e.g. wheel up) moves back along the bar, negative forward.
	ActionBar->CycleActive(Axis > 0.f ? -1 : 1);
}

void AStartechShipPawn::OnThrottle(const FInputActionValue& Value) { SetThrottleInput(IsHubOpen() ? 0.f : Value.Get<float>()); }
void AStartechShipPawn::OnTurn(const FInputActionValue& Value)     { RawTurnAxis = IsHubOpen() ? 0.f : Value.Get<float>(); }
void AStartechShipPawn::OnVertical(const FInputActionValue& Value) { SetVerticalInput(IsHubOpen() ? 0.f : Value.Get<float>()); }

float AStartechShipPawn::GetBankLimit() const
{
	return bInCockpit ? CockpitMaxBankAngle : MaxBankAngle;
}

void AStartechShipPawn::OnToggleView()
{
	SetCockpitView(!bInCockpit);
}

void AStartechShipPawn::SetCockpitView(const bool bEnable)
{
	const bool bChanged = bInCockpit != bEnable;
	bInCockpit = bEnable;
	ChaseCamera->SetActive(!bEnable);
	CockpitCamera->SetActive(bEnable);

	// Hides the hull from your own view only. Its shadow and collision stay.
	ShipMesh->SetOwnerNoSee(bEnable);
	CockpitRoot->SetVisibility(bEnable, true);

	LookYaw = 0.f;
	LookPitch = 0.f;
	CockpitCamera->SetRelativeRotation(FRotator::ZeroRotator);
	
	HeadOffset = FVector::ZeroVector;
	CockpitCamera->SetRelativeLocation(EyeLocation);

	if (bChanged)
	{
		STARTECH_HUD_LOG(TEXT("OnCameraModeChanged: %s"), bInCockpit ? TEXT("first person") : TEXT("third person"));
		OnCameraModeChanged.Broadcast(bInCockpit);
	}
}

void AStartechShipPawn::Tick(const float DeltaSeconds)
{
	TimeSinceTurnInput = FMath::IsNearlyZero(RawTurnAxis)
		? FMath::Min(TimeSinceTurnInput + DeltaSeconds, FireAssistResumeDelay)
		: 0.f;

	const UTargetableComponent* const StanceTarget = GetStanceTarget();
	const UTargetableComponent* const AssistTarget = StanceTarget ? nullptr : GetFireAssistTarget();

	// Routed every tick, so a held key switches meaning cleanly when a lock starts or ends.
	RouteFlightInput(StanceTarget, AssistTarget);

	const float YawBeforeFlight = GetActorRotation().Yaw;

	// Before the flight model, so this frame's flight uses this frame's boost.
	UpdateBoost(DeltaSeconds);

	// Runs the flight model.
	Super::Tick(DeltaSeconds);

	if (AssistTarget)
	{
		CompensateLookForShipYaw(FMath::FindDeltaAngleDegrees(YawBeforeFlight, GetActorRotation().Yaw));
	}

	if (StanceTarget)
	{
		UpdateCombatCamera(*StanceTarget, DeltaSeconds);
	}

	UpdateLook();
	UpdateCockpitCamera(DeltaSeconds);

	WeaponSystem->UpdateFiring(IsTriggerHeld(), TargetingComponent->GetAimPoint(), DeltaSeconds);

	// After the flight model has moved the ship this tick.
	UpdateSectorTimer(DeltaSeconds);
}

void AStartechShipPawn::UpdateSectorTimer(const float DeltaSeconds)
{
	const ASectorBounds* const Bounds = SectorBounds.Get();
	if ((!Bounds && !bForceOutOfBounds) || HealthComponent->IsDead())
	{
		return;
	}

	const bool bOutside = bForceOutOfBounds || (!IsDocked() && !Bounds->IsInside(GetActorLocation()));
	if (bOutside != bOutsideSector)
	{
		bOutsideSector = bOutside;
		TimeOutsideSector = 0.f;
		if (bOutside)
		{
			OnLeftSector.Broadcast();
		}
		else
		{
			OnReturnedToSector.Broadcast();
		}

		STARTECH_HUD_LOG(TEXT("OnOutOfBoundsChanged: %s (%.1f s allowed)"), bOutside ? TEXT("true") : TEXT("false"),
			GetTimeAllowedOutside());
		OnOutOfBoundsChanged.Broadcast(bOutside);
	}

	if (!bOutsideSector)
	{
		return;
	}

	TimeOutsideSector += DeltaSeconds;
	if (TimeOutsideSector >= GetTimeAllowedOutside())
	{
		// More than everything the ship has left, with no instigator, so the normal death flow runs.
		const float Lethal = HealthComponent->GetHull() + HealthComponent->GetShield() + 1.f;
		UGameplayStatics::ApplyDamage(this, Lethal, nullptr, nullptr, UDamageType::StaticClass());
	}
}

void AStartechShipPawn::ResetSectorTimer()
{
	const bool bWasOutside = bOutsideSector;
	bOutsideSector = false;
	TimeOutsideSector = 0.f;

	if (bWasOutside)
	{
		STARTECH_HUD_LOG(TEXT("OnOutOfBoundsChanged: false (reset)"));
		OnOutOfBoundsChanged.Broadcast(false);
	}
}

void AStartechShipPawn::SetForceOutOfBounds(const bool bForce)
{
	// Picked up by the next UpdateSectorTimer, which fires the usual events.
	bForceOutOfBounds = bForce;
}

float AStartechShipPawn::GetTimeAllowedOutside() const
{
	const ASectorBounds* const Bounds = SectorBounds.Get();
	return Bounds ? Bounds->GetTimeAllowedOutside() : FallbackTimeAllowedOutside;
}

float AStartechShipPawn::GetOutOfBoundsSecondsRemaining() const
{
	if (!bOutsideSector)
	{
		return 0.f;
	}
	return FMath::Max(GetTimeAllowedOutside() - TimeOutsideSector, 0.f);
}

void AStartechShipPawn::UpdateCockpitCamera(const float DeltaSeconds)
{
	// Tracked in both views so the first cockpit frame doesn't see a huge jump.
	const FVector ShipAcceleration = DeltaSeconds > 0.f
		? (GetShipVelocity() - LastVelocity) / DeltaSeconds
		: FVector::ZeroVector;
	LastVelocity = GetShipVelocity();

	if (!bInCockpit)
	{
		return;
	}

	// The head moves against acceleration: back on throttle, outward in turns, down when climbing.
	const FVector LocalAcceleration = GetActorTransform().InverseTransformVectorNoScale(ShipAcceleration);
	const FVector TargetOffset = (-LocalAcceleration * HeadSwayScale).GetClampedToMaxSize(MaxHeadSway);
	HeadOffset = FMath::VInterpTo(HeadOffset, TargetOffset, DeltaSeconds, HeadSwayResponse);
	CockpitCamera->SetRelativeLocation(EyeLocation + HeadOffset);

	// Speed widening against the normal top speed (so a boost doesn't push it), plus the boost's own widening.
	const float ForwardRatio = FMath::Clamp(
		FVector::DotProduct(GetShipVelocity(), GetLevelForward()) / GetNormalMaxForwardSpeed(), 0.f, 1.f);
	const float BoostFeel = FMath::SmoothStep(0.f, 1.f, Boost->GetBoostAlpha());
	CockpitCamera->SetFieldOfView(CockpitBaseFOV + CockpitSpeedFOVBoost * ForwardRatio
		+ UStartechSettings::Get().Boost.FieldOfViewWidening * BoostFeel);
}
const UTargetableComponent* AStartechShipPawn::GetStanceTarget() const
{
	if (!TargetingComponent || TargetingComponent->GetLockState() != ETargetLockState::Hard)
	{
		return nullptr;
	}

	const UTargetableComponent* const Target = TargetingComponent->GetCurrentTarget();
	return IsValid(Target) ? Target : nullptr;
}

const UTargetableComponent* AStartechShipPawn::GetFireAssistTarget() const
{
	// A/D must have been released for the resume delay. Any A/D input means the player is steering.
	const bool bSteeringIdle = FMath::IsNearlyZero(RawTurnAxis) && TimeSinceTurnInput >= FireAssistResumeDelay;

	if (!bEnableFireFacingAssist || !IsTriggerHeld() || !bSteeringIdle || IsInCombatStance()
		|| !TargetingComponent || TargetingComponent->GetLockState() != ETargetLockState::Soft)
	{
		return nullptr;
	}

	const UTargetableComponent* const Target = TargetingComponent->GetCurrentTarget();
	return IsValid(Target) ? Target : nullptr;
}

void AStartechShipPawn::RouteFlightInput(const UTargetableComponent* const StanceTarget,
	const UTargetableComponent* const AssistTarget)
{
	if (StanceTarget)
	{
		SetFacingLocation(StanceTarget->GetAimLocation(), FacingTurnRate);
		SetStrafeInput(RawTurnAxis);
		SetTurnInput(0.f);
	}
	else if (AssistTarget)
	{
		// Only active with A/D released, so nothing here can strafe.
		SetFacingLocation(AssistTarget->GetAimLocation(), FireAssistTurnRate);
		SetTurnInput(0.f);
		SetStrafeInput(0.f);
	}
	else
	{
		ClearFacingLocation();
		SetTurnInput(RawTurnAxis);
		SetStrafeInput(0.f);
	}
}

void AStartechShipPawn::UpdateCombatCamera(const UTargetableComponent& StanceTarget, const float DeltaSeconds)
{
	const FVector ToTarget = StanceTarget.GetAimLocation() - GetActorLocation();
	const float TargetPitch = FMath::Clamp(ToTarget.Rotation().Pitch, -LookPitchLimit, LookPitchLimit);

	LookYaw = FMath::FInterpTo(LookYaw, 0.f, DeltaSeconds, CombatCameraInterpSpeed);
	LookPitch = FMath::FInterpTo(LookPitch, TargetPitch, DeltaSeconds, CombatCameraInterpSpeed);
}

void AStartechShipPawn::CompensateLookForShipYaw(const float ShipYawChange)
{
	// Without this the chase camera swings with the ship, the target leaves the soft-lock cone,
	// and the lock that started the turn is lost.
	LookYaw -= ShipYawChange;
	LookYaw = bInCockpit
		? FMath::Clamp(LookYaw, -CockpitLookYawLimit, CockpitLookYawLimit)
		: FRotator::NormalizeAxis(LookYaw);
}

void AStartechShipPawn::OnLook(const FInputActionValue& Value)
{
	// The camera tracks the target in combat stance, and the cursor is in use while the hub is open.
	if (IsInCombatStance() || IsHubOpen())
	{
		return;
	}

	const FVector2D Look = Value.Get<FVector2D>();

	LookPitch = FMath::Clamp(LookPitch + Look.Y * LookSensitivity, -LookPitchLimit, LookPitchLimit);
	LookYaw += Look.X * LookSensitivity;
	LookYaw = bInCockpit
		? FMath::Clamp(LookYaw, -CockpitLookYawLimit, CockpitLookYawLimit)
		: FRotator::NormalizeAxis(LookYaw);
}

void AStartechShipPawn::UpdateLook()
{
	// A tumbling wreck's yaw swings about: the chase camera keeps the heading it had at the death.
	if (!ShipDeath->IsAlive())
	{
		return;
	}

	if (bInCockpit)
	{
		// Relative to the ship: this is your head turning inside the cockpit.
		CockpitCamera->SetRelativeRotation(FRotator(LookPitch, LookYaw, 0.f));
	}
	else
	{
		// Follows the ship's heading, plus the mouse offset, and stays level.
		CameraBoom->SetRelativeRotation(FRotator(LookPitch, GetActorRotation().Yaw + LookYaw, 0.f));
	}
}