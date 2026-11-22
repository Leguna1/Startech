#pragma once

#include "CoreMinimal.h"
#include "LootBundle.h"
#include "LootCollector.h"
#include "StartechShipBase.h"
#include "UI/Hub/HubTab.h"
#include "StartechShipPawn.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class ALootPickup;
class ASectorBounds;
class UInventoryComponent;
class UStationHubWidget;
struct FKey;
class UInputAction;
class UInputMappingContext;
class UTargetableComponent;
class UTargetingComponent;
class UTargetRangeComponent;
class UActionBarComponent;
class UShipBoostComponent;
class UShipUpgradeStatsComponent;
class UWarpTransitionComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootCollected, const FLootBundle&, Taken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerLifeEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSectorCrossing);

/** Broadcast when the ship enters or leaves the out-of-bounds state. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOutOfBoundsChanged, bool, bOutside);

/** Broadcast when the view switches between the cockpit (first person) and the chase camera. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCameraModeChanged, bool, bFirstPerson);

/** Broadcast when the ship docks or undocks. StationName is empty on undocking. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDockedChanged, bool, bDocked, FText, StationName);

/** Broadcast when the station hub opens or closes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHubOpenChanged, bool, bOpen);

UCLASS()
class STARTECH_API AStartechShipPawn : public AStartechShipBase, public ILootCollector
{
	GENERATED_BODY()

public:
	AStartechShipPawn();

	// Credits and items go to the inventory (items up to its capacity), ammo to the weapon system.
	virtual FLootBundle CollectLoot(const FLootBundle& Loot) override;

	UInventoryComponent* GetInventory() const { return Inventory; }

	UTargetingComponent* GetTargeting() const { return TargetingComponent; }

	/** Holds or releases the trigger, as the fire key does (test pilots). Ignored while the hub is open. */
	void SetFireHeld(const bool bHeld) { bWantsToFire = bHeld && !IsHubOpen(); }

	// Whatever was actually taken from a pickup, for sound and UI.
	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FOnLootCollected OnLootCollected;

	// Undocking closes the station hub.
	virtual void SetDockedStation(AStation* Station) override;

	/**
	 * Opens the station hub (StationHubClass) over the game: only while docked and alive. Input becomes UI-only with
	 * the mouse cursor shown, and flight input is ignored until it closes. The game keeps running. Broadcasts
	 * OnHubOpenChanged(true), which hides the game HUD.
	 */
	void OpenHub();

	/**
	 * Closes the station hub and gives input back to the game (game-only, cursor hidden). The ship stays docked.
	 * Broadcasts OnHubOpenChanged(false), which shows the game HUD again.
	 */
	void CloseHub();

	/** True while the station hub is open. */
	bool IsHubOpen() const { return StationHub != nullptr; }

	/**
	 * Leaves the docked station: moves the ship just outside the station's docking zone, facing away from it, with
	 * zero velocity, which undocks it, and closes the hub. Does nothing when not docked.
	 */
	void UndockFromStation();

	/** Drops every held intent (throttle, turn, fire...), for a screen that takes input away, like the pause menu. */
	void ClearHeldInput() { ClearIntents(); }

	/** True if Key is mapped to InteractAction in the player's current input mappings. */
	bool IsInteractKey(const FKey& Key) const;

	/** The display name of the first key mapped to InteractAction (for example "F"), or empty when none is. */
	FText GetInteractKeyText() const;

	/** The hub tab used last, so the hub reopens on it. Quests at first. */
	EHubTab GetLastHubTab() const { return LastHubTab; }

	/** Remembers the hub tab in use. */
	void SetLastHubTab(EHubTab Tab) { LastHubTab = Tab; }

	// The wreck has exploded: the ship is hidden and waiting to respawn.
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnPlayerLifeEvent OnPlayerDied;

	// Back at a station (or where play began) with full health.
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnPlayerLifeEvent OnPlayerRespawned;

	/**
	 * Brings the dead ship back at Spawn (yaw only) with full health, visible, targetable and with input on, then
	 * broadcasts OnPlayerRespawned. Called by UHomeRespawnSubsystem once the home sector is ready.
	 */
	void RespawnAt(const FTransform& Spawn);

	/**
	 * Puts the living ship at Spawn (yaw only) at rest, as the start of a game does: no revive, no events beyond the
	 * docking a station's zone there causes (collision is cycled so the overlap registers at once).
	 */
	void PlaceAt(const FTransform& Spawn);

	/** Where the ship began play: the respawn point when the sector has no station. */
	const FTransform& GetStartTransform() const { return StartTransform; }

	/** True if the ship took damage within the last UStartechSettings::CombatCooldownSeconds. Warping is refused. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInCombat() const;

	/** Seconds until the ship is out of combat; 0 when it already is. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetCombatSecondsRemaining() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

	/** True in the cockpit (first-person) view. */
	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsInCockpit() const { return bInCockpit; }

	/** Fires on every switch between cockpit and chase view. Not at BeginPlay: play always starts in chase view. */
	UPROPERTY(BlueprintAssignable, Category = "Camera")
	FOnCameraModeChanged OnCameraModeChanged;

	/** Fires on docking (with the station's name) and undocking (empty name), and when switching stations. */
	UPROPERTY(BlueprintAssignable, Category = "Station")
	FOnDockedChanged OnDockedChanged;

	/** Fires when the station hub opens (true) and closes (false). The game HUD hides while it is open. */
	UPROPERTY(BlueprintAssignable, Category = "Station")
	FOnHubOpenChanged OnHubOpenChanged;

	/** The component that rates the current target's range against the player's. */
	UTargetRangeComponent* GetTargetRange() const { return TargetRange; }

	/** The action bar: its layout, slot states and slot keys. */
	UActionBarComponent* GetActionBar() const { return ActionBar; }

	/** The boost: its energy pool and speed multiplier. */
	UShipBoostComponent* GetBoost() const { return Boost; }

	/** Applies the upgrade levels to this ship's systems. */
	UShipUpgradeStatsComponent* GetUpgradeStats() const { return UpgradeStats; }

	/**
	 * True when the boost key may act: alive, not warping, and neither the station hub nor the pause menu open.
	 * Otherwise a held boost stops as if released.
	 */
	bool IsBoostAllowed() const;

	// Hard locked on a valid target: the ship faces it and A/D strafe.
	bool IsInCombatStance() const { return GetStanceTarget() != nullptr; }

	// Alive, undocked and outside the level's ASectorBounds. Always false with no sector.
	UFUNCTION(BlueprintPure, Category = "Sector")
	bool IsOutsideSector() const { return bOutsideSector; }

	/** Seconds left before leaving the sector destroys the ship; 0 while inside. Read per frame for the countdown. */
	UFUNCTION(BlueprintPure, Category = "Sector")
	float GetOutOfBoundsSecondsRemaining() const;

	/**
	 * Test hook: while set, the ship counts as out of bounds wherever it is (docked or not), with the usual countdown
	 * and destruction; with no ASectorBounds in the level the countdown uses FallbackTimeAllowedOutside.
	 */
	void SetForceOutOfBounds(bool bForce);

	/** Fires once on each change of IsOutsideSector, including the reset to inside on death and respawn. */
	UPROPERTY(BlueprintAssignable, Category = "Sector")
	FOnOutOfBoundsChanged OnOutOfBoundsChanged;

	// Fired once each time the ship crosses out of the sector, for sound.
	UPROPERTY(BlueprintAssignable, Category = "Sector")
	FOnSectorCrossing OnLeftSector;

	// Fired once each time the ship flies back in. Not fired on death or respawn.
	UPROPERTY(BlueprintAssignable, Category = "Sector")
	FOnSectorCrossing OnReturnedToSector;

protected:
	virtual void BeginPlay() override;

	/** Unregisters the ship's save participants, while the components still hold the latest state. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// At hull 0: stops the ship (it keeps drifting), releases the lock, locks input and leaves the cockpit view.
	virtual void HandleDeath(AActor* Killer) override;

	// After the dying state: drops part of the cargo where the wreck exploded and starts the respawn sequence.
	virtual void HandleDyingFinished(const FVector& Location) override;

	// Seconds from death to respawn.
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0"))
	float RespawnDelay = 2.f;

	// On death, each mineral stack loses a random fraction between these two, dropped as a pickup.
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float DeathDropMinFraction = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float DeathDropMaxFraction = 0.5f;

	// The pickup the dropped cargo goes into. Without one, nothing is dropped.
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TSubclassOf<ALootPickup> DeathLootPickupClass;

	// Seconds of world time after the death before the dropped cargo is gone, whichever sector the player is in.
	// 0 keeps it until collected.
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0", Units = "s"))
	float DeathLootLifetime = 120.f;

	// Cargo lost outside the play area is clamped to the play-area extent minus this margin on each axis.
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0"))
	float DeathLootPlayAreaMargin = 5000.f;

	/** The station hub opened by the interact key while docked: WBP_StationHub. */
	UPROPERTY(EditDefaultsOnly, Category = "Station")
	TSubclassOf<UStationHubWidget> StationHubClass;
	virtual float GetBankLimit() const override;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> ChaseCamera;

	// Parent for interior meshes and screens. Add them under it in BP_Ship.
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> CockpitRoot;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> CockpitCamera;

	UPROPERTY(VisibleAnywhere, Category = "Targeting")
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<UInventoryComponent> Inventory;

	/** Rates the current target's weapon range against the player's, for the reticle's range colour. */
	UPROPERTY(VisibleAnywhere, Category = "Targeting")
	TObjectPtr<UTargetRangeComponent> TargetRange;

	/** The action bar: slot keys 1-6 (DA_ActionBarLayout) and the slot states the HUD draws. */
	UPROPERTY(VisibleAnywhere, Category = "Weapons")
	TObjectPtr<UActionBarComponent> ActionBar;

	/** The boost (UStartechSettings::BoostInputAction, Left Shift): driven from this ship's Tick. */
	UPROPERTY(VisibleAnywhere, Category = "Flight")
	TObjectPtr<UShipBoostComponent> Boost;

	/** Applies UShipUpgradeSubsystem's stats (hull, shield, cargo, engines, weapons, boost) to this ship. */
	UPROPERTY(VisibleAnywhere, Category = "Upgrades")
	TObjectPtr<UShipUpgradeStatsComponent> UpgradeStats;

	/** Seconds allowed outside when the out-of-bounds state is forced in a level with no ASectorBounds. */
	UPROPERTY(EditDefaultsOnly, Category = "Sector", meta = (ClampMin = "0", Units = "s"))
	float FallbackTimeAllowedOutside = 10.f;

	// Lower bank in the cockpit, since the whole view tilts with the ship.
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CockpitMaxBankAngle = 12.f;

	// How far the head moves per cm/s^2 of acceleration, and the cap in cm.
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float HeadSwayScale = 0.004f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxHeadSway = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float HeadSwayResponse = 5.f;

	// Field of view widens with forward speed.
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CockpitBaseFOV = 95.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CockpitSpeedFOVBoost = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> VerticalAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleViewAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LockTargetAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	// Axis1D, e.g. the mouse wheel. Positive selects the previous action bar slot, negative the next; empty slots
	// and slots with no ammo are skipped.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CycleSlotAction;

	// While docked, opens the station hub; the hub itself closes on this key or Escape.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// Degrees of camera turn per unit of mouse movement.
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LookSensitivity = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float LookPitchLimit = 60.f;

	// The chase cam can orbit all the way round; your head in the cockpit can't.
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CockpitLookYawLimit = 100.f;

	// How quickly the camera swings onto the target in combat stance.
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CombatCameraInterpSpeed = 6.f;

	// In free flight, while firing at a soft-locked target, gently turn the nose toward it.
	UPROPERTY(EditDefaultsOnly, Category = "Flight")
	bool bEnableFireFacingAssist = true;

	UPROPERTY(EditDefaultsOnly, Category = "Flight", meta = (ClampMin = "0"))
	float FireAssistTurnRate = 45.f;

	// Seconds A/D must be released before the assist takes over again.
	UPROPERTY(EditDefaultsOnly, Category = "Flight", meta = (ClampMin = "0"))
	float FireAssistResumeDelay = 0.25f;

private:
	void OnThrottle(const FInputActionValue& Value);
	void OnTurn(const FInputActionValue& Value);
	void OnVertical(const FInputActionValue& Value);
	void OnToggleView();
	void OnFireStarted();
	void OnFireCompleted();
	void OnInteract();

	/** UStartechSettings::PauseInputAction (Escape): opens the pause menu in flight. */
	void OnPause();

	/** UStartechSettings::BoostInputAction pressed: starts boosting when allowed. */
	void OnBoostStarted();

	/** UStartechSettings::BoostInputAction released. */
	void OnBoostCompleted();

	/**
	 * Runs the boost for this frame, applies its multiplier to the flight model, and its feel to the cameras: the
	 * chase camera's field of view and position lag. The cockpit's widening is in UpdateCockpitCamera.
	 */
	void UpdateBoost(float DeltaSeconds);
	void OnCycleSlot(const FInputActionValue& Value);

	// The trigger as the weapons see it: released while docked.
	bool IsTriggerHeld() const;

	// Drops every held intent, so nothing carries on once input stops arriving.
	void ClearIntents();

	void UpdateCockpitCamera(float DeltaSeconds);

	void SetCockpitView(bool bEnable);

	bool bInCockpit = false;

	/** The cockpit view was on when the ship died; RespawnAt turns it back on. */
	bool bCockpitBeforeDeath = false;
	bool bWantsToFire = false;

	/** The chase camera's field of view as set in BP_Ship, before the boost widens it. */
	float ChaseBaseFOV = 90.f;
	FVector EyeLocation = FVector::ZeroVector;
	FVector HeadOffset = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;

	void OnLook(const FInputActionValue& Value);
	void UpdateLook();

	// The hard-locked target, or null when not in combat stance.
	const UTargetableComponent* GetStanceTarget() const;

	// The soft-locked target the fire facing assist should turn toward, or null when the assist is off.
	const UTargetableComponent* GetFireAssistTarget() const;

	// Sends the A/D axis to turning or strafing, and sets or clears the facing point.
	// Priority: combat stance, then fire assist, then free flight.
	void RouteFlightInput(const UTargetableComponent* StanceTarget, const UTargetableComponent* AssistTarget);

	// Keeps the camera's world yaw fixed while the assist turns the ship underneath it.
	void CompensateLookForShipYaw(float ShipYawChange);

	// Swings LookYaw/LookPitch onto the target. UpdateLook then applies them.
	void UpdateCombatCamera(const UTargetableComponent& StanceTarget, float DeltaSeconds);

	// Raw A/D value; routed to turn or strafe each tick.
	float RawTurnAxis = 0.f;

	// Time since A/D was last non-zero, capped at FireAssistResumeDelay.
	float TimeSinceTurnInput = 0.f;

	/** Moves part of each mineral stack into a pickup at Location. Returns true if a pickup was dropped. */
	bool DropCargo(const FVector& Location);

	/** Respawn timer callback: hands the respawn to UHomeRespawnSubsystem, or respawns in place without one. */
	void Respawn();

	/** Health callback: any damage taken starts or extends combat. */
	UFUNCTION()
	void HandleDamageTaken(float Amount, bool bHitShield, FVector SourceLocation);

	/** The warp component's CanBeginWarp rule: refuses a warp while in combat. */
	bool CanWarp(FName DestinationSectorId, FText& OutReason) const;

	/** Warp callback: the ship is invulnerable from here until the warp finishes. */
	UFUNCTION()
	void HandleWarpStarted(FName DestinationSectorId);

	/** Warp callback: ends the warp's invulnerability, keeping the docked one. */
	UFUNCTION()
	void HandleWarpFinished(FName SectorId);

	/** The warp component BP_Ship adds, bound in BeginPlay. */
	TWeakObjectPtr<UWarpTransitionComponent> BoundWarp;

	/** True from OnWarpStarted until OnWarpFinished: invulnerable like when docked. */
	bool bWarpProtected = false;

	/** World time the ship last took damage; negative for never (or since the last respawn). */
	double LastDamageTime = -1.0;

	FTimerHandle RespawnTimer;
	FTransform StartTransform;

	// Counts time outside the sector, fires the crossing events, and destroys the ship when time runs out.
	void UpdateSectorTimer(float DeltaSeconds);

	// Back to inside with a zeroed timer, without firing OnReturnedToSector. For death and respawn.
	// Fires OnOutOfBoundsChanged(false) if the ship was outside.
	void ResetSectorTimer();

	/** Seconds the ship may spend outside: the bounds' setting, or FallbackTimeAllowedOutside without bounds. */
	float GetTimeAllowedOutside() const;

	/** Set by SetForceOutOfBounds. */
	bool bForceOutOfBounds = false;

	// Found once in BeginPlay. Null means no limits.
	TWeakObjectPtr<ASectorBounds> SectorBounds;

	// Where cargo lost at DeathLocation is dropped: unchanged inside the play area, otherwise clamped into it.
	FVector GetDeathDropLocation(const FVector& DeathLocation) const;

	bool bOutsideSector = false;
	float TimeOutsideSector = 0.f;

	/** The open station hub, or null. */
	UPROPERTY(Transient)
	TObjectPtr<UStationHubWidget> StationHub;

	/** The hub tab used last. */
	EHubTab LastHubTab = EHubTab::Quests;

	/** Startech.Wallet, .Cargo and .Arsenal, registered with USaveSubsystem from BeginPlay to EndPlay. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> SaveParticipants;

	float LookYaw = 0.f;
	float LookPitch = 0.f;
};
