#include "StartechTargetDummy.h"

#include "HealthComponent.h"
#include "TargetableComponent.h"

void AStartechTargetDummy::BeginPlay()
{
	Super::BeginPlay();

	if (UHealthComponent* const Health = FindComponentByClass<UHealthComponent>())
	{
		Health->OnDeath.AddUniqueDynamic(this, &AStartechTargetDummy::HandleDeath);
	}
}

void AStartechTargetDummy::HandleDeath(AActor* const Killer)
{
	if (UTargetableComponent* const Targetable = FindComponentByClass<UTargetableComponent>())
	{
		Targetable->SetTargetable(false);
	}
}
