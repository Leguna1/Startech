#include "Environment/PlayAreaSyncComponent.h"

#include "Components/BoxComponent.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProcSectorLog.h"

UPlayAreaSyncComponent::UPlayAreaSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayAreaSyncComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	USectorDirectorSubsystem* Director = World != nullptr ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	if (Director == nullptr)
	{
		return;
	}

	Director->OnSectorReady.AddUniqueDynamic(this, &UPlayAreaSyncComponent::HandleSectorReady);
	BoundDirector = Director;

	// The first sector may have finished before this actor began play.
	if (!Director->GetCurrentSectorId().IsNone())
	{
		ApplyPlayArea();
	}
}

void UPlayAreaSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USectorDirectorSubsystem* Director = BoundDirector.Get())
	{
		Director->OnSectorReady.RemoveDynamic(this, &UPlayAreaSyncComponent::HandleSectorReady);
	}
	BoundDirector.Reset();

	Super::EndPlay(EndPlayReason);
}

void UPlayAreaSyncComponent::ApplyPlayArea()
{
	const USectorDirectorSubsystem* Director = BoundDirector.Get();
	if (Director == nullptr)
	{
		return;
	}

	UBoxComponent* Box = FindBox();
	if (Box == nullptr)
	{
		if (!bWarnedMissingBox)
		{
			bWarnedMissingBox = true;
			UE_LOG(LogProcSector, Warning, TEXT("PlayAreaSyncComponent on '%s': no box component '%s' found; the bounds will not follow the play area."),
				*GetNameSafe(GetOwner()), BoxComponentName.IsNone() ? TEXT("(first)") : *BoxComponentName.ToString());
		}
		return;
	}

	Box->SetBoxExtent(Director->GetPlayAreaExtent() + ExtentPadding);
}

void UPlayAreaSyncComponent::HandleSectorReady(FName SectorId)
{
	ApplyPlayArea();
}

UBoxComponent* UPlayAreaSyncComponent::FindBox() const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}

	if (BoxComponentName.IsNone())
	{
		return Owner->FindComponentByClass<UBoxComponent>();
	}

	TInlineComponentArray<UBoxComponent*> Boxes(Owner);
	UBoxComponent* const* Found = Boxes.FindByPredicate([this](const UBoxComponent* Box)
	{
		return Box != nullptr && Box->GetFName() == BoxComponentName;
	});
	return Found != nullptr ? *Found : nullptr;
}
