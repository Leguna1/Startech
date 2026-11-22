#include "Core/SectorDirectorSubsystem.h"

#include "Data/EnvironmentPreset.h"
#include "Data/PlayAreaMath.h"
#include "Data/ProcSectorSettings.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "Factories/DefaultCatalogFactory.h"
#include "Factories/EnvironmentParamsFactory.h"
#include "Factories/SectorGenStepFactory.h"
#include "Factories/StarSystemSkyFactory.h"
#include "HAL/PlatformTime.h"
#include "ProcSectorLog.h"

void USectorDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UProcSectorSettings& Settings = UProcSectorSettings::Get();
	if (!Settings.bAutoGenerateOnBeginPlay)
	{
		return;
	}

	EnsureInitialized();

	FName StartSectorId = Settings.StartSectorId;
	if (!Catalog->ContainsSector(StartSectorId) && !Catalog->Sectors.IsEmpty())
	{
		StartSectorId = Catalog->Sectors[0].SectorId;
	}

	RequestSector(StartSectorId);
}

void USectorDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Phase == ESectorPhase::Idle)
	{
		// The playable sector is maintained by the steps that need it (respawns, for example).
		UWorld* const World = GetWorld();
		if (World != nullptr && !CurrentSectorId.IsNone())
		{
			for (USectorGenStep* const Step : Steps)
			{
				if (Step != nullptr)
				{
					Step->TickLive(*World, DeltaTime);
				}
			}
		}
		return;
	}

	const double BudgetSeconds = static_cast<double>(UProcSectorSettings::Get().GenerationBudgetMs) / 1000.0;
	const double DeadlineSeconds = FPlatformTime::Seconds() + BudgetSeconds;

	if (Phase == ESectorPhase::TearingDown)
	{
		TearDownCurrentSector();
		Phase = ESectorPhase::Generating;
		StepIndex = 0;
		PendingTimings.Reset();
		BeginCurrentStep();
	}

	while (Phase == ESectorPhase::Generating)
	{
		if (!Steps.IsValidIndex(StepIndex))
		{
			FinishGeneration();
			break;
		}

		USectorGenStep* Step = Steps[StepIndex];

		const double SliceStart = FPlatformTime::Seconds();
		const bool bStepDone = Step == nullptr || Step->TickSlice(Context, DeadlineSeconds);
		StepAccumulatedSeconds += FPlatformTime::Seconds() - SliceStart;

		if (bStepDone)
		{
			FSectorStepTiming& Timing = PendingTimings.AddDefaulted_GetRef();
			Timing.StepName = Step != nullptr ? Step->GetStepName() : NAME_None;
			Timing.Milliseconds = static_cast<float>(StepAccumulatedSeconds * 1000.0);

			++StepIndex;
			BeginCurrentStep();
		}

		if (FPlatformTime::Seconds() >= DeadlineSeconds)
		{
			break;
		}
	}
}

TStatId USectorDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USectorDirectorSubsystem, STATGROUP_Tickables);
}

bool USectorDirectorSubsystem::RequestSector(FName SectorId)
{
	EnsureInitialized();

	if (Phase != ESectorPhase::Idle)
	{
		UE_LOG(LogProcSector, Warning, TEXT("RequestSector('%s') ignored: a regeneration is already running."), *SectorId.ToString());
		return false;
	}

	const FSectorDefinition* Definition = Catalog->FindSector(SectorId);
	if (Definition == nullptr)
	{
		UE_LOG(LogProcSector, Warning, TEXT("RequestSector('%s') ignored: no such sector in the catalog."), *SectorId.ToString());
		return false;
	}

	PreviousSectorId = CurrentSectorId;
	PendingSectorId = SectorId;

	Context.World = GetWorld();
	Context.PreviousSectorId = PreviousSectorId;
	Context.Definition = *Definition;
	Context.Seed = ResolveSeed(*Definition);
	Context.State = StateStore.GetCopy(SectorId);
	Context.Params = Definition->Preset != nullptr
		? FEnvironmentParamsFactory::Build(*Definition->Preset, Context.Seed)
		: FEnvironmentParams();

	// When the sector belongs to a planet of the star system, its sun and neighbours come from the system's layout.
	FStarSystemSkyFactory::Apply(Catalog->StarSystem, Definition->BodyId, Context.Params);

	// Last, because it depends on the final sun direction set just above.
	FEnvironmentParamsFactory::ResolvePlanetshine(Context.Params);

	Phase = ESectorPhase::TearingDown;
	OnSectorTransitionStarted.Broadcast(PreviousSectorId, PendingSectorId);
	return true;
}

bool USectorDirectorSubsystem::RegenerateCurrentSector()
{
	return RequestSector(CurrentSectorId);
}

void USectorDirectorSubsystem::RerollSeeds()
{
	if (Phase != ESectorPhase::Idle)
	{
		return;
	}

	// Ids stored in the state belong to the old layouts, so they must be forgotten with them.
	SeedSalt = FMath::Rand();
	StateStore.Reset();
	RegenerateCurrentSector();
}

bool USectorDirectorSubsystem::IsBusy() const
{
	return Phase != ESectorPhase::Idle;
}

ESectorPhase USectorDirectorSubsystem::GetPhase() const
{
	return Phase;
}

FName USectorDirectorSubsystem::GetCurrentSectorId() const
{
	return CurrentSectorId;
}

FName USectorDirectorSubsystem::GetPreviousSectorId() const
{
	return PreviousSectorId;
}

int32 USectorDirectorSubsystem::GetCurrentSeed() const
{
	return Context.Seed;
}

FVector USectorDirectorSubsystem::GetPlayAreaExtent() const
{
	return Context.Definition.PlayAreaExtent;
}

const FEnvironmentParams& USectorDirectorSubsystem::GetCurrentParams() const
{
	return Context.Params;
}

const USectorCatalog* USectorDirectorSubsystem::GetCatalog()
{
	EnsureInitialized();
	return Catalog;
}

const USectorCatalog* USectorDirectorSubsystem::GetCatalog() const
{
	return Catalog;
}

bool USectorDirectorSubsystem::GetArrivalTransform(FName FromSectorId, FTransform& OutTransform) const
{
	const FGateLink* Link = Context.Definition.Gates.FindByPredicate([FromSectorId](const FGateLink& Gate)
	{
		return Gate.DestinationSectorId == FromSectorId;
	});

	if (Link == nullptr)
	{
		return false;
	}

	const FVector GateLocation = FPlayAreaMath::ToWorld(Link->NormalizedLocation, Context.Definition.PlayAreaExtent);

	FVector ToCentre = (-GateLocation).GetSafeNormal();
	if (ToCentre.IsNearlyZero())
	{
		ToCentre = FVector::ForwardVector;
	}

	const FVector Location = GateLocation + ToCentre * UProcSectorSettings::Get().GateArrivalDistance;
	OutTransform = FTransform(ToCentre.Rotation(), Location);
	return true;
}

void USectorDirectorSubsystem::RecordAsteroidRemoved(const int32 AsteroidId)
{
	if (!CurrentSectorId.IsNone())
	{
		StateStore.RecordAsteroidRemoved(CurrentSectorId, AsteroidId);
	}
}

bool USectorDirectorSubsystem::ConsumeSpawnedActor(AActor* Actor)
{
	if (Actor == nullptr || Phase != ESectorPhase::Idle)
	{
		return false;
	}

	for (USectorGenStep* Step : Steps)
	{
		int32 SpawnId = INDEX_NONE;
		bool bRemember = false;

		if (Step != nullptr && Step->TryConsumeActor(*Actor, SpawnId, bRemember))
		{
			if (bRemember && !CurrentSectorId.IsNone())
			{
				StateStore.RecordSpawnConsumed(CurrentSectorId, SpawnId);
			}
			return true;
		}
	}
	return false;
}

int32 USectorDirectorSubsystem::CountLiveSpawns(const TSubclassOf<AActor> ActorClass) const
{
	const UClass* const Class = ActorClass.Get();
	if (Class == nullptr)
	{
		return 0;
	}

	int32 Count = 0;
	for (const USectorGenStep* const Step : Steps)
	{
		if (Step != nullptr)
		{
			Count += Step->CountLiveActors(Class);
		}
	}
	return Count;
}

int32 USectorDirectorSubsystem::CountPendingRespawns() const
{
	int32 Count = 0;
	for (const USectorGenStep* const Step : Steps)
	{
		if (Step != nullptr)
		{
			Count += Step->CountPendingRespawns();
		}
	}
	return Count;
}

const TArray<FSectorStepTiming>& USectorDirectorSubsystem::GetLastStepTimings() const
{
	return LastTimings;
}

float USectorDirectorSubsystem::GetLastGenerationMs() const
{
	return LastGenerationMs;
}

const FSectorStateStore& USectorDirectorSubsystem::GetStateStore() const
{
	return StateStore;
}

FSectorStateStore& USectorDirectorSubsystem::GetMutableStateStore()
{
	return StateStore;
}

bool USectorDirectorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USectorDirectorSubsystem::EnsureInitialized()
{
	if (bInitializedPipeline)
	{
		return;
	}
	bInitializedPipeline = true;

	const UProcSectorSettings& Settings = UProcSectorSettings::Get();

	Catalog = Settings.Catalog.LoadSynchronous();
	if (Catalog == nullptr || Catalog->Sectors.IsEmpty())
	{
		UE_LOG(LogProcSector, Log, TEXT("No sector catalog assigned in the settings; using the built-in test catalog."));
		Catalog = FDefaultCatalogFactory::CreateTransientCatalog(this);
	}

	Steps.Reset();
	for (USectorGenStep* Step : FSectorGenStepFactory::CreatePipeline(this, Settings.StepClasses))
	{
		Steps.Add(Step);
	}
}

void USectorDirectorSubsystem::TearDownCurrentSector()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (USectorGenStep* Step : Steps)
	{
		if (Step != nullptr)
		{
			Step->Clear(*World);
		}
	}
}

void USectorDirectorSubsystem::BeginCurrentStep()
{
	StepAccumulatedSeconds = 0.0;

	if (Steps.IsValidIndex(StepIndex) && Steps[StepIndex] != nullptr)
	{
		Steps[StepIndex]->Begin(Context);
	}
}

void USectorDirectorSubsystem::FinishGeneration()
{
	LastTimings = PendingTimings;
	LastGenerationMs = 0.0f;
	for (const FSectorStepTiming& Timing : LastTimings)
	{
		LastGenerationMs += Timing.Milliseconds;
	}

	CurrentSectorId = PendingSectorId;
	Phase = ESectorPhase::Idle;

	UE_LOG(LogProcSector, Log, TEXT("Sector '%s' ready (seed %d, %.2f ms of generation work)."),
		*CurrentSectorId.ToString(), Context.Seed, LastGenerationMs);

	OnSectorReady.Broadcast(CurrentSectorId);
}

int32 USectorDirectorSubsystem::ResolveSeed(const FSectorDefinition& Definition) const
{
	return static_cast<int32>(static_cast<uint32>(Definition.Seed) + static_cast<uint32>(SeedSalt));
}
