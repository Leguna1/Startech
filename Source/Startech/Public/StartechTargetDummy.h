#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StartechTargetDummy.generated.h"

// Parent for non-ship targets. Its components (health, targetable) are added in the Blueprint;
// this class only links them so that dying makes the actor untargetable.
UCLASS(Abstract)
class STARTECH_API AStartechTargetDummy : public AActor
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// Bound to the health component's OnDeath, if there is one. The base version only makes the
	// actor untargetable, which releases any lock on it. Overrides must call Super.
	UFUNCTION()
	virtual void HandleDeath(AActor* Killer);
};
