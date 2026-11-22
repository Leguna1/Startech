#include "Population/RelevanceSubsystem.h"

#include "Data/ProcSectorSettings.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Population/RelevanceComponent.h"

void URelevanceSubsystem::Register(URelevanceComponent& Component)
{
	Components.AddUnique(&Component);
}

void URelevanceSubsystem::Unregister(URelevanceComponent& Component)
{
	Components.RemoveSwap(&Component);
}

void URelevanceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector ViewerLocation;
	if (Components.IsEmpty() || !GetViewerLocation(ViewerLocation))
	{
		return;
	}

	const int32 Checks = FMath::Min(UProcSectorSettings::Get().RelevanceChecksPerFrame, Components.Num());
	for (int32 Check = 0; Check < Checks && !Components.IsEmpty(); ++Check)
	{
		if (Cursor >= Components.Num())
		{
			Cursor = 0;
		}

		URelevanceComponent* Component = Components[Cursor].Get();
		if (Component == nullptr)
		{
			Components.RemoveAtSwap(Cursor);
			continue;
		}

		if (Component->IsEvaluationEnabled())
		{
			Component->Evaluate(ViewerLocation);
		}
		++Cursor;
	}
}

TStatId URelevanceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URelevanceSubsystem, STATGROUP_Tickables);
}

int32 URelevanceSubsystem::GetTrackedCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<URelevanceComponent>& Component : Components)
	{
		if (Component.IsValid() && Component->IsEvaluationEnabled())
		{
			++Count;
		}
	}
	return Count;
}

int32 URelevanceSubsystem::GetAwakeCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<URelevanceComponent>& Component : Components)
	{
		if (Component.IsValid() && Component->IsEvaluationEnabled() && Component->IsAwake())
		{
			++Count;
		}
	}
	return Count;
}

bool URelevanceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool URelevanceSubsystem::GetViewerLocation(FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (PlayerController == nullptr)
	{
		return false;
	}

	if (const APawn* Pawn = PlayerController->GetPawn())
	{
		OutLocation = Pawn->GetActorLocation();
		return true;
	}

	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(OutLocation, ViewRotation);
	return true;
}
