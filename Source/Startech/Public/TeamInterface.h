#pragma once

#include "CoreMinimal.h"
#include "Team.h"
#include "UObject/Interface.h"
#include "TeamInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTeamInterface : public UInterface
{
	GENERATED_BODY()
};

// Anything that belongs to a side. C++ only.
class STARTECH_API ITeamInterface
{
	GENERATED_BODY()

public:
	virtual ETeam GetTeam() const = 0;
};
