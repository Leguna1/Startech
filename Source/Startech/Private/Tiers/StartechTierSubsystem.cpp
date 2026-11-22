#include "Tiers/StartechTierSubsystem.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Engine/World.h"
#include "PlayerProfileSubsystem.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechTier, Log, All);

namespace StartechTier
{
	/** The values of a tier missing from the settings: every multiplier 1, no mineral weights. */
	const FSectorTierData& NeutralData()
	{
		static const FSectorTierData Neutral = []
		{
			FSectorTierData Data;
			Data.DisplayName = NSLOCTEXT("SectorTier", "Unknown", "?");
			return Data;
		}();
		return Neutral;
	}
}

UStartechTierSubsystem* UStartechTierSubsystem::Get(const UObject* const WorldContextObject)
{
	const UWorld* const World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UStartechTierSubsystem>() : nullptr;
}

void UStartechTierSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Bound here rather than at begin play: the first RequestSector can come from any begin play, and the transition
	// event that sets the spawn tier must not be missed.
	USectorDirectorSubsystem* const Director = Collection.InitializeDependency<USectorDirectorSubsystem>();
	if (!Director)
	{
		return;
	}

	Director->OnSectorTransitionStarted.AddUniqueDynamic(this, &UStartechTierSubsystem::HandleTransitionStarted);
	Director->OnSectorReady.AddUniqueDynamic(this, &UStartechTierSubsystem::HandleSectorReady);
	BoundDirector = Director;
}

void UStartechTierSubsystem::Deinitialize()
{
	if (USectorDirectorSubsystem* const Director = BoundDirector.Get())
	{
		Director->OnSectorTransitionStarted.RemoveDynamic(this, &UStartechTierSubsystem::HandleTransitionStarted);
		Director->OnSectorReady.RemoveDynamic(this, &UStartechTierSubsystem::HandleSectorReady);
	}
	BoundDirector.Reset();

	Super::Deinitialize();
}

bool UStartechTierSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

const FSectorTierData& UStartechTierSubsystem::GetCurrentTierData() const
{
	return GetTierData(CurrentTier);
}

const FSectorTierData& UStartechTierSubsystem::GetSpawnTierData() const
{
	return GetTierData(SpawnTier);
}

int32 UStartechTierSubsystem::GetTierForSector(const FName SectorId) const
{
	const FSectorTierAssignment* const Assignment = UStartechSettings::Get().SectorTiers.FindByPredicate(
		[SectorId](const FSectorTierAssignment& Entry) { return Entry.SectorId == SectorId; });
	if (Assignment)
	{
		return FMath::Max(Assignment->Tier, 1);
	}

	if (!SectorId.IsNone() && !WarnedSectors.Contains(SectorId))
	{
		WarnedSectors.Add(SectorId);
		UE_LOG(LogStartechTier, Warning, TEXT("Sector %s is not in UStartechSettings::SectorTiers; it counts as tier 1."),
			*SectorId.ToString());
	}
	return 1;
}

const FSectorTierData* UStartechTierSubsystem::FindTierData(const int32 Tier)
{
	const TArray<FSectorTierData>& Table = UStartechSettings::Get().TierData;
	return Table.IsValidIndex(Tier - 1) ? &Table[Tier - 1] : nullptr;
}

const FSectorTierData& UStartechTierSubsystem::GetTierData(const int32 Tier) const
{
	if (const FSectorTierData* const Found = FindTierData(Tier))
	{
		return *Found;
	}

	const TArray<FSectorTierData>& Table = UStartechSettings::Get().TierData;

	if (!WarnedTiers.Contains(Tier))
	{
		WarnedTiers.Add(Tier);
		UE_LOG(LogStartechTier, Warning, TEXT("Tier %d is not in UStartechSettings::TierData (%d entries); neutral values used."),
			Tier, Table.Num());
	}
	return StartechTier::NeutralData();
}

void UStartechTierSubsystem::HandleTransitionStarted(const FName FromSectorId, const FName ToSectorId)
{
	SpawnTier = GetTierForSector(ToSectorId);
	UE_LOG(LogStartechTier, Log, TEXT("Generating %s: spawns use tier %d."), *ToSectorId.ToString(), SpawnTier);
}

void UStartechTierSubsystem::HandleSectorReady(const FName SectorId)
{
	const int32 NewTier = GetTierForSector(SectorId);
	SpawnTier = NewTier;

	const bool bChanged = !bHasSector || NewTier != CurrentTier;
	CurrentTier = NewTier;
	bHasSector = true;

	if (bChanged)
	{
		UE_LOG(LogStartechTier, Log, TEXT("Sector %s is tier %d."), *SectorId.ToString(), CurrentTier);
		OnTierChanged.Broadcast(CurrentTier);
	}

	UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(this);
	if (Profile && CurrentTier > Profile->GetHighestTierVisited())
	{
		Profile->SetHighestTierVisited(CurrentTier);
		UE_LOG(LogStartechTier, Log, TEXT("First visit to tier %d."), CurrentTier);
		OnFirstVisitToTier.Broadcast(CurrentTier);
	}
}
