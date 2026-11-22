#include "StartechQuestBridge.h"

#include "AmmoData.h"
#include "Bounties/BountyBoardSubsystem.h"
#include "Core/SectorDirectorSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EnemyData.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "QuestTags.h"
#include "Runtime/QuestSubsystem.h"
#include "StartechEnemyShip.h"
#include "StartechGameplayTags.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "TimerManager.h"
#include "WeaponData.h"
#include "WeaponSystemComponent.h"

DEFINE_LOG_CATEGORY(LogStartechQuest);

#define LOCTEXT_NAMESPACE "StartechQuestBridge"

namespace
{
	/** Keeps the bridge's quest-call depth raised for one scope, so events raised inside it are held. */
	struct FQuestCallScope
	{
		explicit FQuestCallScope(int32& InDepth) : Depth(InDepth) { ++Depth; }
		~FQuestCallScope() { --Depth; }

		/** The bridge's QuestCallDepth. */
		int32& Depth;
	};
}

UStartechQuestBridge* UStartechQuestBridge::Get(const UObject* const WorldContextObject)
{
	const UWorld* const World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UStartechQuestBridge>() : nullptr;
}

bool UStartechQuestBridge::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UStartechQuestBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->SetInventoryProvider(this);
		Quests->SetRewardGranter(this);
		Quests->SetTurnInGate(FQuestTurnInGate::CreateUObject(this, &UStartechQuestBridge::CanTurnIn));
		UE_LOG(LogStartechQuest, Log, TEXT("Quest bridge registered (inventory, rewards, turn-in gate)."));
	}
	else
	{
		UE_LOG(LogStartechQuest, Warning, TEXT("No UQuestSubsystem; quests will not see the game."));
	}

	// The director requests the first sector during its own OnWorldBeginPlay but finishes it in a later Tick,
	// so binding here also catches the first OnSectorReady.
	if (USectorDirectorSubsystem* const Director = InWorld.GetSubsystem<USectorDirectorSubsystem>())
	{
		Director->OnSectorReady.AddUniqueDynamic(this, &UStartechQuestBridge::HandleSectorReady);
	}
}

void UStartechQuestBridge::Deinitialize()
{
	UnbindPlayerShip();

	if (const UWorld* const World = GetWorld())
	{
		if (USectorDirectorSubsystem* const Director = World->GetSubsystem<USectorDirectorSubsystem>())
		{
			Director->OnSectorReady.RemoveDynamic(this, &UStartechQuestBridge::HandleSectorReady);
		}
		World->GetTimerManager().ClearTimer(FlushTimer);
	}

	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->SetInventoryProvider(nullptr);
		Quests->SetRewardGranter(nullptr);
		Quests->SetTurnInGate(FQuestTurnInGate());
	}

	HeldEvents.Reset();
	Super::Deinitialize();
}

void UStartechQuestBridge::BindPlayerShip(AStartechShipPawn& Ship)
{
	if (PlayerShip.Get() == &Ship)
	{
		return;
	}

	UnbindPlayerShip();
	PlayerShip = &Ship;

	if (UInventoryComponent* const Inventory = Ship.GetInventory())
	{
		ItemCountHandle = Inventory->OnItemCountChanged.AddUObject(this, &UStartechQuestBridge::HandleItemCountChanged);
	}
	if (UWeaponSystemComponent* const Weapons = Ship.GetWeaponSystem())
	{
		AmmoAddedHandle = Weapons->OnAmmoAdded.AddUObject(this, &UStartechQuestBridge::HandleAmmoAdded);
	}
	Ship.OnDockedChanged.AddUniqueDynamic(this, &UStartechQuestBridge::HandleDockedChanged);

	UE_LOG(LogStartechQuest, Log, TEXT("Quest bridge bound to player ship %s."), *Ship.GetName());

	// The ship can start inside a station's zone and dock before binding.
	if (Ship.IsDocked())
	{
		HandleDockedChanged(true, FText::GetEmpty());
	}
}

void UStartechQuestBridge::UnbindPlayerShip()
{
	AStartechShipPawn* const Ship = PlayerShip.Get();
	if (Ship)
	{
		if (UInventoryComponent* const Inventory = Ship->GetInventory())
		{
			Inventory->OnItemCountChanged.Remove(ItemCountHandle);
		}
		if (UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem())
		{
			Weapons->OnAmmoAdded.Remove(AmmoAddedHandle);
		}
		Ship->OnDockedChanged.RemoveDynamic(this, &UStartechQuestBridge::HandleDockedChanged);
	}

	ItemCountHandle.Reset();
	AmmoAddedHandle.Reset();
	PlayerShip.Reset();
}

void UStartechQuestBridge::ReportEnemyDestroyed(const AStartechEnemyShip& Enemy, const AActor* const Killer)
{
	// The player's lasers and beams pass the ship's controller as instigator and the ship as causer, so the health
	// component names the ship as killer. Enemies have no controller and cannot hurt each other; a kill with no
	// killer (Startech.Quest.KillNearestEnemy, for one) is not the player's.
	const AStartechShipPawn* const Ship = PlayerShip.Get();
	if (!Ship || Killer != Ship)
	{
		UE_LOG(LogStartechQuest, Log, TEXT("%s destroyed by %s: not the player, not reported."), *Enemy.GetName(),
			*GetNameSafe(Killer));
		return;
	}

	const UEnemyData* const Data = Enemy.GetEnemyData();
	FQuestEvent Event;
	Event.EventTag = QuestTags::Event_TargetDestroyed;
	Event.SubjectTags = Data ? Data->EnemyTags : FGameplayTagContainer();
	Event.ContextId = GetCurrentSectorId();
	Event.Amount = 1;

	if (Event.SubjectTags.IsEmpty())
	{
		UE_LOG(LogStartechQuest, Warning, TEXT("%s (%s) has no EnemyTags; its kill matches no quest."), *Enemy.GetName(),
			*GetNameSafe(Data));
	}

	UE_LOG(LogStartechQuest, Log, TEXT("TargetDestroyed %s [%s] in %s"), *Enemy.GetName(),
		*Event.SubjectTags.ToStringSimple(), *Event.ContextId.ToString());
	SendEvent(Event);
}

int32 UStartechQuestBridge::GetItemCount(const FGameplayTag& ItemTag) const
{
	const AStartechShipPawn* const Ship = PlayerShip.Get();
	if (!Ship || !ItemTag.IsValid())
	{
		return 0;
	}

	if (const UItemData* const Item = FindHeldItem(ItemTag))
	{
		return Ship->GetInventory() ? Ship->GetInventory()->GetItemCount(Item) : 0;
	}

	const UAmmoData* const Ammo = Cast<UAmmoData>(UStartechSettings::Get().FindItem(ItemTag));
	const UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem();
	return Ammo && Weapons ? FMath::FloorToInt32(Weapons->GetAmmoCount(Ammo)) : 0;
}

bool UStartechQuestBridge::RemoveItems(const FGameplayTag& ItemTag, const int32 Count)
{
	const FQuestCallScope Scope(QuestCallDepth);

	const AStartechShipPawn* const Ship = PlayerShip.Get();
	if (!Ship || Count < 0)
	{
		return false;
	}

	if (const UItemData* const Item = FindHeldItem(ItemTag))
	{
		UInventoryComponent* const Inventory = Ship->GetInventory();
		const bool bRemoved = Inventory && Inventory->RemoveItem(Item, Count);
		UE_LOG(LogStartechQuest, Log, TEXT("Turn-in takes %d x %s: %s"), Count, *ItemTag.ToString(),
			bRemoved ? TEXT("done") : TEXT("not enough held"));
		return bRemoved;
	}

	if (const UAmmoData* const Ammo = Cast<UAmmoData>(UStartechSettings::Get().FindItem(ItemTag)))
	{
		UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem();
		return Weapons && Weapons->GetAmmoCount(Ammo) >= Count && Weapons->ConsumeAmmo(Ammo, Count);
	}

	return Count == 0;
}

void UStartechQuestBridge::GrantReward(const FQuestReward& Reward)
{
	const FQuestCallScope Scope(QuestCallDepth);

	const AStartechShipPawn* const Ship = PlayerShip.Get();
	if (!Ship || Reward.Amount <= 0)
	{
		UE_LOG(LogStartechQuest, Warning, TEXT("Reward %d x %s not granted: %s."), Reward.Amount,
			*Reward.RewardTag.ToString(), Ship ? TEXT("nothing to give") : TEXT("no player ship"));
		return;
	}

	if (Reward.RewardTag.MatchesTagExact(StartechTags::Reward_Credits))
	{
		if (UInventoryComponent* const Inventory = Ship->GetInventory())
		{
			Inventory->AddCredits(Reward.Amount);
			UE_LOG(LogStartechQuest, Log, TEXT("Reward: %d credits."), Reward.Amount);
		}
		return;
	}

	UTradeableData* const Good = UStartechSettings::Get().FindItem(Reward.RewardTag);
	if (UItemData* const Item = Cast<UItemData>(Good))
	{
		UInventoryComponent* const Inventory = Ship->GetInventory();
		const int32 Added = Inventory ? Inventory->AddItem(Item, Reward.Amount) : 0;
		UE_LOG(LogStartechQuest, Log, TEXT("Reward: %d x %s."), Added, *Reward.RewardTag.ToString());
		if (Added < Reward.Amount)
		{
			UE_LOG(LogStartechQuest, Warning, TEXT("Reward: %d x %s did not fit in the cargo hold and were lost."),
				Reward.Amount - Added, *Reward.RewardTag.ToString());
		}
		return;
	}

	if (UAmmoData* const Ammo = Cast<UAmmoData>(Good))
	{
		if (UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem())
		{
			Weapons->AddAmmo(Ammo, static_cast<float>(Reward.Amount));
			UE_LOG(LogStartechQuest, Log, TEXT("Reward: %d x %s."), Reward.Amount, *Reward.RewardTag.ToString());
		}
		return;
	}

	if (UWeaponData* const Weapon = Cast<UWeaponData>(Good))
	{
		if (UInventoryComponent* const Inventory = Ship->GetInventory())
		{
			Inventory->AddWeapon(Weapon, Reward.Amount);
			UE_LOG(LogStartechQuest, Log, TEXT("Reward: %d x %s (spare weapon)."), Reward.Amount, *Reward.RewardTag.ToString());
		}
		return;
	}

	UE_LOG(LogStartechQuest, Warning,
		TEXT("Reward %d x %s not granted: no good in Startech settings' ItemCatalog has that ItemTag."),
		Reward.Amount, *Reward.RewardTag.ToString());
}

void UStartechQuestBridge::HandleSectorReady(const FName SectorId)
{
	FQuestEvent Event;
	Event.EventTag = QuestTags::Event_LocationReached;
	Event.ContextId = SectorId;
	Event.Amount = 1;

	UE_LOG(LogStartechQuest, Log, TEXT("LocationReached %s"), *SectorId.ToString());
	SendEvent(Event);
}

void UStartechQuestBridge::HandleDockedChanged(const bool bDocked, FText /*StationName*/)
{
	if (!bDocked)
	{
		return;
	}
	if (bDockRefreshSuppressed)
	{
		UE_LOG(LogStartechQuest, Log, TEXT("Docked by the start flow: bounty board kept as saved."));
		return;
	}

	const UWorld* const World = GetWorld();
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UBountyBoardSubsystem* const Board = GameInstance ? GameInstance->GetSubsystem<UBountyBoardSubsystem>() : nullptr)
	{
		Board->Refresh();
		UE_LOG(LogStartechQuest, Log, TEXT("Docked: bounty board refreshed (%d offers)."), Board->GetOffers().Num());
	}
}

void UStartechQuestBridge::HandleItemCountChanged(const UItemData* const Item, const int32 Delta)
{
	if (Item && Delta != 0)
	{
		// A loss is reported with Amount 0: Collect objectives only re-read the inventory on the event.
		ReportItemEvent(*Item, FMath::Max(Delta, 0));
	}
}

void UStartechQuestBridge::HandleAmmoAdded(const UAmmoData* const Ammo, const float Amount)
{
	if (Ammo && Amount > 0.f)
	{
		ReportItemEvent(*Ammo, FMath::Max(FMath::FloorToInt32(Amount), 1));
	}
}

bool UStartechQuestBridge::CanTurnIn(const FName /*QuestId*/, FText& OutReason) const
{
	const AStartechShipPawn* const Ship = PlayerShip.Get();
	if (Ship && Ship->IsDocked())
	{
		return true;
	}

	OutReason = LOCTEXT("DockToTurnIn", "Dock at the station to turn in");
	return false;
}

void UStartechQuestBridge::ReportItemEvent(const UTradeableData& Good, const int32 Amount)
{
	if (!Good.ItemTag.IsValid())
	{
		const bool bWarned = WarnedUntagged.ContainsByPredicate([&Good](const TWeakObjectPtr<const UTradeableData>& Each)
		{
			return Each.Get() == &Good;
		});
		if (!bWarned)
		{
			WarnedUntagged.Add(&Good);
			UE_LOG(LogStartechQuest, Warning, TEXT("%s has no ItemTag; quests cannot see it."), *Good.GetName());
		}
		return;
	}

	FQuestEvent Event;
	Event.EventTag = QuestTags::Event_ItemCollected;
	Event.SubjectTags.AddTag(Good.ItemTag);
	Event.ContextId = GetCurrentSectorId();
	Event.Amount = Amount;

	UE_LOG(LogStartechQuest, Log, TEXT("ItemCollected %s x%d in %s"), *Good.ItemTag.ToString(), Amount,
		*Event.ContextId.ToString());
	SendEvent(Event);
}

void UStartechQuestBridge::SendEvent(const FQuestEvent& Event)
{
	if (QuestCallDepth > 0)
	{
		HeldEvents.Add(Event);
		if (UWorld* const World = GetWorld(); World && !FlushTimer.IsValid())
		{
			FlushTimer = World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &UStartechQuestBridge::FlushHeldEvents));
		}
		return;
	}

	if (UQuestSubsystem* const Quests = GetQuests())
	{
		Quests->ReportEvent(Event);
	}
}

void UStartechQuestBridge::FlushHeldEvents()
{
	FlushTimer.Invalidate();

	// A copy: reporting may raise new events.
	const TArray<FQuestEvent> Events = MoveTemp(HeldEvents);
	HeldEvents.Reset();
	for (const FQuestEvent& Event : Events)
	{
		SendEvent(Event);
	}
}

UQuestSubsystem* UStartechQuestBridge::GetQuests() const
{
	const UWorld* const World = GetWorld();
	const UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr;
}

FName UStartechQuestBridge::GetCurrentSectorId() const
{
	const UWorld* const World = GetWorld();
	const USectorDirectorSubsystem* const Director = World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	return Director ? Director->GetCurrentSectorId() : NAME_None;
}

const UItemData* UStartechQuestBridge::FindHeldItem(const FGameplayTag& Tag) const
{
	const AStartechShipPawn* const Ship = PlayerShip.Get();
	const UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr;
	if (Inventory && Tag.IsValid())
	{
		for (const FItemStack& Stack : Inventory->GetItems())
		{
			if (Stack.Item && Stack.Item->ItemTag.MatchesTagExact(Tag))
			{
				return Stack.Item;
			}
		}
	}

	// Not held now: a listed mineral still counts as a mineral (held 0).
	return Cast<UItemData>(UStartechSettings::Get().FindItem(Tag));
}

#undef LOCTEXT_NAMESPACE
