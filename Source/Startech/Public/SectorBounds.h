#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SectorBounds.generated.h"

class UBoxComponent;

// The playable space. A player ship that stays outside the box for TimeAllowedOutside is destroyed.
// Place one per level; with none, there are no limits.
UCLASS()
class STARTECH_API ASectorBounds : public AActor
{
	GENERATED_BODY()

public:
	ASectorBounds();

	bool IsInside(const FVector& Location) const;

	// Location clamped into the box and pulled in by a small margin, so the result is safely inside.
	FVector GetClosestPointInside(const FVector& Location) const;

	FVector GetCenter() const;

	float GetTimeAllowedOutside() const { return TimeAllowedOutside; }

protected:
	// The playable space. No collision.
	UPROPERTY(VisibleAnywhere, Category = "Sector")
	TObjectPtr<UBoxComponent> Area;

	// Seconds a ship may stay outside before it is destroyed.
	UPROPERTY(EditAnywhere, Category = "Sector", meta = (ClampMin = "0"))
	float TimeAllowedOutside = 10.f;
};
