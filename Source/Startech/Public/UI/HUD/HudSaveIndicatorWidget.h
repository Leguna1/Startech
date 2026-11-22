#pragma once

#include "CoreMinimal.h"
#include "UI/HUD/HudElementWidget.h"
#include "HudSaveIndicatorWidget.generated.h"

class USaveSubsystem;

/**
 * Above the cargo and credits panel: a small right-aligned line showing the autosave. "Saving…" in secondary text
 * while a save is in flight, then "Saved" for a second; nothing otherwise. A failed save shows "Save failed" in the
 * Danger colour for five seconds. Follows SaveFramework's OnSaveStarted and OnSaveCompleted; not the pawn.
 */
UCLASS()
class STARTECH_API UHudSaveIndicatorWidget : public UHudElementWidget
{
	GENERATED_BODY()

protected:
	/** Binds to the save subsystem. */
	virtual void NativeConstruct() override;

	/** Unbinds from it. */
	virtual void NativeDestruct() override;

	/** Hides the line once its time is up. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	/** What the line shows. */
	enum class EState : uint8
	{
		/** Nothing. */
		Hidden,
		/** "Saving…", while a save is in flight. */
		Saving,
		/** "Saved", until HideAt. */
		Saved,
		/** "Save failed", until HideAt. */
		Failed
	};

	/** Save started: shows "Saving…". */
	UFUNCTION()
	void HandleSaveStarted(FName Reason);

	/** Save completed: "Saved" for SavedSeconds, or "Save failed" for FailedSeconds. */
	UFUNCTION()
	void HandleSaveCompleted(bool bSuccess, FName Reason);

	/** Sets the state and repaints. */
	void SetState(EState NewState, double Seconds);

	/** The game instance's save subsystem, or null (for example in the designer). */
	USaveSubsystem* GetSaves() const;

	/** Current state. */
	EState State = EState::Hidden;

	/** HUD time the Saved or Failed line goes away. */
	double HideAt = 0.0;
};
