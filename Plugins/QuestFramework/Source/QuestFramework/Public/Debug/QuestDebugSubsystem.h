#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QuestDebugSubsystem.generated.h"

/**
 * Draws an on-screen overlay of the active quests with their objective progress and time left. Only reads from
 * UQuestSubsystem. Toggle with the console variable "Quest.Debug 1" (0 hides it). Non-shipping builds only.
 */
UCLASS()
class QUESTFRAMEWORK_API UQuestDebugSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Prints the overlay lines for this frame when the console variable is on. */
	virtual void Tick(float DeltaTime) override;

	/** Stat id required by the tickable interface. */
	virtual TStatId GetStatId() const override;

protected:
	/** Game and Play-In-Editor worlds of non-shipping builds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** Prints one overlay line for one frame, under its own key so it is replaced in place. */
	static void PrintLine(int32 LineIndex, const FString& Text, const FColor& Color);

	/** First on-screen message key used by the overlay, chosen to avoid clashing with other debug messages. */
	static constexpr int32 FirstMessageKey = 78100;
};
