#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SectorDebugSubsystem.generated.h"

/**
 * Draws an on-screen overlay with the current sector, seed, play-area size, generation timings,
 * pending respawns, pool usage and relevance counts. Only reads from the other subsystems.
 * Toggle with the console variable "ProcSector.Debug 1" (0 hides it); "ProcSector.Debug 2" also draws the play-area box in the world.
 */
UCLASS()
class PROCSECTOR_API USectorDebugSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Prints the overlay lines for this frame when the console variable is on, and draws the play-area box when it is 2 or more. */
	virtual void Tick(float DeltaTime) override;

	/** Stat id required by the tickable interface. */
	virtual TStatId GetStatId() const override;

protected:
	/** Limits the subsystem to game and Play-In-Editor worlds. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Prints one overlay line. Each line uses its own key so it is replaced in place instead of scrolling. */
	static void PrintLine(const int32 LineIndex, const FString& Text, const FColor& Color);

	/** Console variable value from which the play-area box is drawn as well as the overlay. */
	static constexpr int32 PlayAreaBoxLevel = 2;

	/** First on-screen message key used by the overlay, chosen to avoid clashing with other debug messages. */
	static constexpr int32 FirstMessageKey = 77100;
};
