#include "Death/ShipDeathPreloadSubsystem.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Death/ShipDeathTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnemyData.h"
#include "StartechEnemyShip.h"
#include "StartechSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechDeathPreload, Log, All);

void UShipDeathPreloadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (USectorDirectorSubsystem* const Director = InWorld.GetSubsystem<USectorDirectorSubsystem>())
	{
		Director->OnSectorReady.AddUniqueDynamic(this, &UShipDeathPreloadSubsystem::HandleSectorReady);
		BoundDirector = Director;
	}

	// The defaults and the player's, before any sector is ready (and in levels without ProcSector).
	Preload(TEXT("begin play"));
}

void UShipDeathPreloadSubsystem::Deinitialize()
{
	if (USectorDirectorSubsystem* const Director = BoundDirector.Get())
	{
		Director->OnSectorReady.RemoveDynamic(this, &UShipDeathPreloadSubsystem::HandleSectorReady);
	}
	BoundDirector.Reset();

	if (Handle.IsValid())
	{
		Handle->ReleaseHandle();
		Handle.Reset();
	}

	Super::Deinitialize();
}

bool UShipDeathPreloadSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UShipDeathPreloadSubsystem::HandleSectorReady(const FName SectorId)
{
	Preload(*SectorId.ToString());
}

void UShipDeathPreloadSubsystem::AddEffects(const FShipDeathEffects& Effects, TArray<FSoftObjectPath>& OutPaths)
{
	for (const FSoftObjectPath& Path : { Effects.DeathExplosionFX.ToSoftObjectPath(),
		Effects.SecondaryExplosionFX.ToSoftObjectPath(), Effects.DeathSound.ToSoftObjectPath() })
	{
		if (!Path.IsNull())
		{
			OutPaths.AddUnique(Path);
		}
	}
}

void UShipDeathPreloadSubsystem::Preload(const TCHAR* const Reason)
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FSoftObjectPath> Paths;
	AddEffects(UStartechSettings::Get().Death.DefaultEffects, Paths);

	// The ranged enemy's charged-shot hooks too, for the same reason.
	const FChargedShotEffects& Charged = UStartechSettings::Get().ChargedShot;
	for (const FSoftObjectPath& Path : { Charged.ChargeFX.ToSoftObjectPath(), Charged.ChargeSound.ToSoftObjectPath(),
		Charged.ProjectileFX.ToSoftObjectPath(), Charged.FireSound.ToSoftObjectPath(), Charged.ImpactFX.ToSoftObjectPath(),
		Charged.ImpactSound.ToSoftObjectPath() })
	{
		if (!Path.IsNull())
		{
			Paths.AddUnique(Path);
		}
	}

	// Every ship in the world: the player and each enemy type, pooled ones included.
	TArray<const UEnemyData*> Types;
	int32 Ships = 0;
	for (TActorIterator<AStartechShipBase> It(World); It; ++It)
	{
		++Ships;
		const AStartechEnemyShip* const Enemy = Cast<AStartechEnemyShip>(*It);
		const UEnemyData* const Data = Enemy ? Enemy->GetEnemyData() : nullptr;
		if (Enemy && Data && Types.Contains(Data))
		{
			continue;
		}
		if (Data)
		{
			Types.Add(Data);
		}
		AddEffects(It->GetDeathEffects(), Paths);
	}

	// The new request first, so assets in both stay loaded while the old handle is released.
	const TSharedPtr<FStreamableHandle> Previous = Handle;
	Handle = Paths.Num() > 0
		? UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority)
		: nullptr;
	if (Previous.IsValid())
	{
		Previous->ReleaseHandle();
	}

	PreloadedCount = Paths.Num();
	UE_LOG(LogStartechDeathPreload, Log, TEXT("[Death] preloading %d death effect asset(s) for %s (%d ship(s), %d enemy type(s))"),
		PreloadedCount, Reason, Ships, Types.Num());
}
