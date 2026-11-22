#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TargetRegistrySubsystem.generated.h"

class UTargetableComponent;

// Every targetable component in the world, so targeting never has to search actors.
UCLASS()
class STARTECH_API UTargetRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(UTargetableComponent* Target);
	void Unregister(UTargetableComponent* Target);

	// May contain stale entries; check each one before use.
	const TArray<TWeakObjectPtr<UTargetableComponent>>& GetTargets() const { return Targets; }

private:
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UTargetableComponent>> Targets;
};
