#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StartechPauseMenuSubsystem.generated.h"

class UPauseMenuWidget;

/**
 * Opens and closes the pause menu (UStartechSettings::PauseMenuWidgetClass, WBP_PauseMenu) in the game map. Opening
 * pauses the game and gives input to the menu (UI-only, cursor); resuming unpauses and hands input back to the game
 * (FStartechInputState::SetGameOnly). Only in flight: not before the game has started, not while dead, respawning or
 * warping, not while the station hub is open (Escape closes the hub there, as before).
 */
UCLASS()
class STARTECH_API UStartechPauseMenuSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of WorldContext's world, or null. */
	static UStartechPauseMenuSubsystem* Get(const UObject* WorldContext);

	/** Opens the pause menu and pauses, when the player is in flight. False when it can't open now. */
	bool OpenPauseMenu();

	/** Closes the pause menu, unpauses and gives input back to the game. */
	void Resume();

	/** True while the pause menu is shown. */
	bool IsPauseMenuOpen() const { return PauseMenu != nullptr; }

	/** Removes the menu without touching the pause (the world is going away). */
	virtual void Deinitialize() override;

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True when the player may pause now. */
	bool CanPause() const;

	/** The menu shown, or null. */
	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenu;
};
