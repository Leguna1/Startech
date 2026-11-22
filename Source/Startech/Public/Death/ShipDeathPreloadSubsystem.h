#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShipDeathPreloadSubsystem.generated.h"

struct FShipDeathEffects;
struct FStreamableHandle;
class USectorDirectorSubsystem;

/**
 * Keeps the death effects loaded so the first death never stutters (UShipDeathComponent loads them synchronously).
 * On world begin play and on every OnSectorReady it requests, asynchronously, UStartechSettings::Death.DefaultEffects,
 * the player's AStartechShipBase::DeathEffects, every enemy type's UEnemyData::DeathEffects found in the world and the
 * charged-shot effects (UStartechSettings::ChargedShot), and holds them until the next request.
 */
UCLASS()
class STARTECH_API UShipDeathPreloadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Number of effect assets in the last request. */
	int32 GetPreloadedCount() const { return PreloadedCount; }

protected:
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleSectorReady(FName SectorId);

	/** Requests every death effect in use, then releases the previous request. */
	void Preload(const TCHAR* Reason);

	static void AddEffects(const FShipDeathEffects& Effects, TArray<FSoftObjectPath>& OutPaths);

	TSharedPtr<FStreamableHandle> Handle;
	TWeakObjectPtr<USectorDirectorSubsystem> BoundDirector;
	int32 PreloadedCount = 0;
};
