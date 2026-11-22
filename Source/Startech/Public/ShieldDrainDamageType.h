#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "ShieldDrainDamageType.generated.h"

// Marker: damage of this type only drains shields and never touches hull.
UCLASS()
class STARTECH_API UShieldDrainDamageType : public UDamageType
{
	GENERATED_BODY()
};
