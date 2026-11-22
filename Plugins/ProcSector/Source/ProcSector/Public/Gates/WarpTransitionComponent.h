#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "WarpTransitionComponent.generated.h"

class APawn;
class APlayerController;

/** Stage of a warp in progress (State pattern, kept as a simple enum-driven machine). */
UENUM(BlueprintType)
enum class EWarpState : uint8
{
	/** Not warping; the player has control. */
	Idle,
	/** The screen is fading to the warp colour. Nothing has been regenerated yet. */
	FadingOut,
	/** The screen is fully covered and the director is rebuilding the level. */
	Generating
};

/** Broadcast when a warp starts. Start warp-tunnel effects and sounds here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarpStarted, FName, DestinationSectorId);

/** Broadcast when the player regains control in the new sector. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarpFinished, FName, SectorId);

/** Broadcast when the game refuses a warp through CanBeginWarp. Reason is the text the game gave, for the HUD. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarpRefused, FText, Reason);

/**
 * Asked by BeginWarp before a warp starts, when bound. Return false to refuse it and fill OutReason with a message
 * for the player.
 */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FCanBeginWarp, FName /* DestinationSectorId */, FText& /* OutReason */);

/**
 * Add to the player's ship. It is the only class that knows the warp sequence:
 * lock input, fade out, ask the director for the new sector, wait until it is ready,
 * move the ship to the arrival gate, fade in, unlock input.
 * The regeneration is hidden because it only starts once the screen is covered.
 */
UCLASS(ClassGroup = (ProcSector), meta = (BlueprintSpawnableComponent))
class PROCSECTOR_API UWarpTransitionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Disables this component's tick; the sequence is driven by timers and the director's event. */
	UWarpTransitionComponent();

	/**
	 * Starts a warp to the sector. Returns false if already warping, the director is busy, the sector is unknown or
	 * CanBeginWarp refuses it; a CanBeginWarp refusal also broadcasts OnWarpRefused.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcSector|Warp")
	bool BeginWarp(FName DestinationSectorId);

	/**
	 * Like BeginWarp, for a warp started by entering Trigger (a jump gate). If CanBeginWarp refuses it, the warp is
	 * retried every RefusedRetrySeconds while the owner still overlaps Trigger, and starts by itself once allowed.
	 * Retries do not broadcast OnWarpRefused again. Retrying stops when the owner leaves Trigger or a warp starts.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcSector|Warp")
	bool BeginWarpFromTrigger(FName DestinationSectorId, AActor* Trigger);

	/** Stops retrying a refused warp, if a retry is pending. */
	UFUNCTION(BlueprintCallable, Category = "ProcSector|Warp")
	void CancelWarpRetry();

	/** True while a warp refused by CanBeginWarp is being retried because the owner is still inside the trigger. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Warp")
	bool IsRetryingWarp() const;

	/**
	 * Optional game rule consulted by BeginWarp before a warp starts (native only). Unbound means every warp is
	 * allowed. Bind it on the owning pawn, for example to refuse warps in combat.
	 */
	FCanBeginWarp CanBeginWarp;

	/** Fires when CanBeginWarp refuses a warp, with its reason. Not fired again by the retries. */
	UPROPERTY(BlueprintAssignable, Category = "ProcSector|Warp")
	FOnWarpRefused OnWarpRefused;

	/** True from BeginWarp until control is returned. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Warp")
	bool IsWarping() const;

	/** Current stage of the warp. */
	UFUNCTION(BlueprintPure, Category = "ProcSector|Warp")
	EWarpState GetWarpState() const;

	/** Fires when a warp starts. */
	UPROPERTY(BlueprintAssignable, Category = "ProcSector|Warp")
	FOnWarpStarted OnWarpStarted;

	/** Fires when the warp has finished and input is restored. */
	UPROPERTY(BlueprintAssignable, Category = "ProcSector|Warp")
	FOnWarpFinished OnWarpFinished;

protected:
	/** Subscribes to the director's OnSectorReady event. */
	virtual void BeginPlay() override;

	/** Unsubscribes and clears timers. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds the screen takes to fade out before regeneration starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp", meta = (ClampMin = "0.05"))
	float FadeOutSeconds = 0.6f;

	/** Seconds the screen takes to fade back in after arrival. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp", meta = (ClampMin = "0.05"))
	float FadeInSeconds = 0.8f;

	/** Shortest time the screen stays covered, so a fast regeneration does not look like a flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp", meta = (ClampMin = "0.0"))
	float MinimumHoldSeconds = 0.75f;

	/** Colour the screen fades to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp")
	FLinearColor FadeColor = FLinearColor::Black;

	/** Seconds between retries of a warp refused by CanBeginWarp while the owner stays inside the gate's trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp", meta = (ClampMin = "0.05", Units = "s"))
	float RefusedRetrySeconds = 0.25f;

private:
	/**
	 * BeginWarp's body. bOutRefusedByGame is set when CanBeginWarp refused; OnWarpRefused is broadcast for that only
	 * when bBroadcastRefusal is true.
	 */
	bool TryBeginWarp(FName DestinationSectorId, bool bBroadcastRefusal, bool& bOutRefusedByGame);

	/** Retry timer callback: stops once the owner has left the trigger, otherwise tries the warp again quietly. */
	void HandleRetryTimer();

	/** Timer callback: the screen is covered, so ask the director to regenerate. */
	void HandleFadeOutFinished();

	/** Director callback: the new sector is ready. Waits out the minimum hold, then completes the arrival. */
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Moves the ship to the arrival gate, fades in, restores input and broadcasts OnWarpFinished. */
	void CompleteArrival();

	/** Fades the owning player's camera between two alpha values. */
	void StartFade(const float FromAlpha, const float ToAlpha, const float Duration) const;

	/** Enables or disables player input on the owning pawn. */
	void SetInputLocked(const bool bLocked) const;

	/** Returns the owner as a pawn, or nullptr. */
	APawn* GetOwnerPawn() const;

	/** Returns the player controller possessing the owner, or nullptr. */
	APlayerController* GetOwnerPlayerController() const;

	/** Current stage of the warp. */
	EWarpState State = EWarpState::Idle;

	/** Sector the warp in progress is travelling to. */
	FName PendingSectorId;

	/** World time at which the screen became fully covered. */
	double HoldStartSeconds = 0.0;

	/** Timer used for the fade-out wait and the minimum hold. */
	FTimerHandle TimerHandle;

	/** Timer for retrying a refused warp. */
	FTimerHandle RetryTimerHandle;

	/** Trigger the owner must stay inside for a refused warp to be retried. */
	TWeakObjectPtr<AActor> RetryTrigger;

	/** Sector the refused warp is retried to. */
	FName RetrySectorId;
};
