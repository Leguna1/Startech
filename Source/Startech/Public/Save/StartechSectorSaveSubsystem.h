#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StartechSectorSaveSubsystem.generated.h"

class USectorsSaveParticipant;

/**
 * Registers the Startech.Sectors participant for the game world's sector director as the world starts, before any
 * sector is requested, and unregisters it when the world begins tearing down (while the director still holds the
 * state, so the latest changes are kept for the next save). Game and PIE worlds only.
 */
UCLASS()
class STARTECH_API UStartechSectorSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Creates the participant for the world's director and registers it. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Registers the participant if Initialize could not yet reach the game instance. */
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Unregisters the participant if teardown has not already done it. */
	virtual void Deinitialize() override;

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Registers Participant with the game instance's save subsystem, once. */
	void TryRegister();

	/** Unregisters Participant, once. */
	void Unregister();

	/** World teardown callback: unregisters while the director still exists. */
	void HandleWorldBeginTearDown(UWorld* World);

	/** Startech.Sectors. */
	UPROPERTY(Transient)
	TObjectPtr<USectorsSaveParticipant> Participant;

	/** True while Participant is registered. */
	bool bRegistered = false;

	/** The teardown delegate binding. */
	FDelegateHandle TearDownHandle;
};
