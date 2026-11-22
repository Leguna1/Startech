#include "LootPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DespawnSubsystem.h"
#include "Engine/World.h"
#include "InventoryComponent.h"
#include "LootCollector.h"
#include "LootTable.h"
#include "Tiers/StartechTierSubsystem.h"
#include "TimerManager.h"

ALootPickup::ALootPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(150.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	PickupSphere->SetCanEverAffectNavigation(false);
	SetRootComponent(PickupSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PickupSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
}

void ALootPickup::SetContents(const FLootBundle& NewContents)
{
	Contents = NewContents;

	if (HasActorBegunPlay())
	{
		TryCollectFromOverlapping();
	}
}

void ALootPickup::BeginPlay()
{
	Super::BeginPlay();

	// Bound here rather than in the constructor so the binding isn't saved into Blueprint defaults.
	PickupSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ALootPickup::HandleBeginOverlap);
	PickupSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ALootPickup::HandleEndOverlap);

	ResetForNewLife();
}

void ALootPickup::OnAcquiredFromPool_Implementation()
{
	ResetForNewLife();
}

void ALootPickup::OnReleasedToPool_Implementation()
{
	StopWaitingForAll();

	// Leftovers of a partly taken pickup must not carry into its next life.
	Contents = FLootBundle();
}

void ALootPickup::ResetForNewLife()
{
	bCollected = false;
	bCollecting = false;

	// Zone spawns are placed straight from the pool, so the pickup fills itself. A table can roll nothing; reroll a
	// few times, and if it is still empty Settle despawns it.
	// The spawn tier: zone loot is placed while the next sector generates, before the current tier switches.
	if (LootTable)
	{
		const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(this);
		const FLootRollModifiers Modifiers = FLootRollModifiers::FromTier(
			Tiers ? Tiers->GetSpawnTierData() : FSectorTierData(), ELootRollSource::World);
		for (int32 Attempt = 0; Attempt < MaxRollAttempts && Contents.IsEmpty(); ++Attempt)
		{
			Contents = LootTable->Roll(Modifiers);
		}
	}

	// Not now: the pool and the drop code fill and place the pickup after this runs, and despawning or collecting
	// in the middle of an acquire would hand the caller an actor that is already back in the pool.
	FTimerManager& Timers = GetWorldTimerManager();
	if (!Timers.TimerExists(SettleTimer))
	{
		SettleTimer = Timers.SetTimerForNextTick(this, &ALootPickup::Settle);
	}
}

void ALootPickup::Settle()
{
	SettleTimer.Invalidate();

	if (bCollected)
	{
		return;
	}

	if (Contents.IsEmpty())
	{
		UDespawnSubsystem::Despawn(this);
		return;
	}

	// Something already inside when the life began (e.g. spawned on top of the player) won't overlap again.
	TryCollectFromOverlapping();
}

void ALootPickup::StartDespawnTimer(const float Seconds)
{
	if (Seconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(DespawnTimer, this, &ALootPickup::HandleDespawnTimerElapsed, Seconds, false);
	}
}

void ALootPickup::HandleDespawnTimerElapsed()
{
	if (!bCollected)
	{
		bCollected = true;
		StopWaitingForAll();
		UDespawnSubsystem::Despawn(this);
	}
}

void ALootPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWaitingForAll();

	Super::EndPlay(EndPlayReason);
}

void ALootPickup::HandleBeginOverlap(UPrimitiveComponent* const OverlappedComponent, AActor* const OtherActor,
	UPrimitiveComponent* const OtherComp, const int32 OtherBodyIndex, const bool bFromSweep, const FHitResult& SweepResult)
{
	TryCollect(OtherActor);
}

void ALootPickup::HandleEndOverlap(UPrimitiveComponent* const OverlappedComponent, AActor* const OtherActor,
	UPrimitiveComponent* const OtherComp, const int32 OtherBodyIndex)
{
	// An actor can overlap through more than one component; only stop once it has fully left.
	if (OtherActor && !IsOverlappingActor(OtherActor))
	{
		StopWaiting(OtherActor);
	}
}

void ALootPickup::HandleCollectorItemsChanged()
{
	if (bCollecting)
	{
		return;
	}

	// A copy, because collecting changes the list and may destroy this pickup.
	const TArray<TWeakObjectPtr<AActor>> Waiting = WaitingCollectors;
	for (const TWeakObjectPtr<AActor>& Weak : Waiting)
	{
		if (bCollected)
		{
			return;
		}

		if (AActor* const Collector = Weak.Get(); Collector && IsOverlappingActor(Collector))
		{
			TryCollect(Collector);
		}
	}
}

void ALootPickup::TryCollectFromOverlapping()
{
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping);
	for (AActor* const Actor : Overlapping)
	{
		if (bCollected)
		{
			return;
		}
		TryCollect(Actor);
	}
}

void ALootPickup::TryCollect(AActor* const Collector)
{
	if (bCollecting || bCollected || !HasActorBegunPlay() || Contents.IsEmpty())
	{
		return;
	}

	ILootCollector* const LootCollector = Cast<ILootCollector>(Collector);
	if (!LootCollector)
	{
		return;
	}

	{
		TGuardValue<bool> CollectingGuard(bCollecting, true);
		Contents = LootCollector->CollectLoot(Contents);
	}

	if (!Contents.IsEmpty())
	{
		// Taken in part or not at all: stay put and try again once it has room.
		StartWaiting(Collector);
		return;
	}

	bCollected = true;
	StopWaitingForAll();
	OnCollected.Broadcast(this, Collector);
	UDespawnSubsystem::Despawn(this);
}

void ALootPickup::StartWaiting(AActor* const Collector)
{
	const bool bAlreadyWaiting = WaitingCollectors.ContainsByPredicate([Collector](const TWeakObjectPtr<AActor>& Weak)
	{
		return Weak.Get() == Collector;
	});
	if (bAlreadyWaiting)
	{
		return;
	}

	WaitingCollectors.Add(Collector);
	if (UInventoryComponent* const Inventory = Collector->FindComponentByClass<UInventoryComponent>())
	{
		Inventory->OnItemsChanged.AddUniqueDynamic(this, &ALootPickup::HandleCollectorItemsChanged);
	}
}

void ALootPickup::StopWaiting(AActor* const Collector)
{
	const int32 Removed = WaitingCollectors.RemoveAllSwap([Collector](const TWeakObjectPtr<AActor>& Weak)
	{
		return Weak.Get() == Collector;
	});
	if (Removed == 0)
	{
		return;
	}

	if (UInventoryComponent* const Inventory = Collector->FindComponentByClass<UInventoryComponent>())
	{
		Inventory->OnItemsChanged.RemoveDynamic(this, &ALootPickup::HandleCollectorItemsChanged);
	}
}

void ALootPickup::StopWaitingForAll()
{
	for (const TWeakObjectPtr<AActor>& Weak : WaitingCollectors)
	{
		if (const AActor* const Collector = Weak.Get())
		{
			if (UInventoryComponent* const Inventory = Collector->FindComponentByClass<UInventoryComponent>())
			{
				Inventory->OnItemsChanged.RemoveDynamic(this, &ALootPickup::HandleCollectorItemsChanged);
			}
		}
	}
	WaitingCollectors.Reset();
}
