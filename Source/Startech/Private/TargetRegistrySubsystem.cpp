#include "TargetRegistrySubsystem.h"

#include "TargetableComponent.h"

void UTargetRegistrySubsystem::Register(UTargetableComponent* const Target)
{
	if (Target)
	{
		Targets.AddUnique(Target);
	}
}

void UTargetRegistrySubsystem::Unregister(UTargetableComponent* const Target)
{
	// Also drops any entries whose component was destroyed without unregistering.
	Targets.RemoveAll([Target](const TWeakObjectPtr<UTargetableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Target;
	});
}
