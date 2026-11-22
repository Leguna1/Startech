#include "Save/ArsenalSaveParticipant.h"

#include "AmmoData.h"
#include "InventoryComponent.h"
#include "Save/StartechSaveConversion.h"
#include "Save/StartechSaveLog.h"
#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"
#include "ShipFitting.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "WeaponData.h"
#include "WeaponMountComponent.h"
#include "WeaponSystemComponent.h"

const FName UArsenalSaveParticipant::ParticipantId(TEXT("Startech.Arsenal"));

FName UArsenalSaveParticipant::GetSaveParticipantId() const
{
	return ParticipantId;
}

int32 UArsenalSaveParticipant::GetSaveVersion() const
{
	// 2: one selected ammo per weapon group (GroupSelections).
	return 2;
}

void UArsenalSaveParticipant::WriteSaveData(TArray<uint8>& OutData) const
{
	static const TCHAR* const Context = TEXT("Startech.Arsenal");

	FStartechArsenalSave Save;
	if (const AStartechShipPawn* const Ship = GetShip())
	{
		TArray<UWeaponMountComponent*> Mounts;
		Ship->GetComponents(Mounts);
		for (const UWeaponMountComponent* const Mount : Mounts)
		{
			FStartechMountWeapon& Entry = Save.Mounts.AddDefaulted_GetRef();
			Entry.MountName = Mount->GetFName();
			Entry.WeaponTag = FStartechSaveConversion::TagOf(Mount->GetFittedWeapon(), Context);
		}
	}
	if (const UInventoryComponent* const Inventory = GetInventory())
	{
		Save.SpareWeapons = FStartechSaveConversion::ToTagged(Inventory->GetWeapons(), Context);
	}
	if (const UWeaponSystemComponent* const Weapons = GetWeaponSystem())
	{
		Save.Ammo = FStartechSaveConversion::ToTagged(Weapons->GetAmmoStacks(), Context);
		Save.SelectedAmmo = FStartechSaveConversion::TagOf(Weapons->GetSelectedAmmo(), Context);
		for (const EWeaponGroup Group : FWeaponGroups::All())
		{
			const FGameplayTag Tag = FStartechSaveConversion::TagOf(Weapons->GetSelectedAmmoForGroup(Group), Context);
			if (Tag.IsValid())
			{
				Save.GroupSelections.Add(Tag);
			}
		}
	}
	FSaveSerialization::StructToBytes(Save, OutData);
}

void UArsenalSaveParticipant::ReadSaveData(const TArray<uint8>& Data, const int32 SavedVersion)
{
	FStartechArsenalSave Save;
	if (!FSaveSerialization::BytesToStruct(Data, Save))
	{
		UE_LOG(LogStartechSave, Error, TEXT("Startech.Arsenal: unreadable record; new-game weapons and ammo instead."));
		OnNewGame();
		return;
	}
	Apply(Save);
}

void UArsenalSaveParticipant::OnNewGame()
{
	const UStartechSettings& Settings = UStartechSettings::Get();
	FStartechArsenalSave Save;
	Save.Mounts = Settings.NewGameMountWeapons;
	Save.Ammo = Settings.NewGameAmmo;
	Save.SelectedAmmo = Settings.NewGameSelectedAmmo;
	Apply(Save);
}

void UArsenalSaveParticipant::Apply(const FStartechArsenalSave& Save) const
{
	static const TCHAR* const Context = TEXT("Startech.Arsenal");

	AStartechShipPawn* const Ship = GetShip();
	UInventoryComponent* const Inventory = GetInventory();
	UWeaponSystemComponent* const Weapons = GetWeaponSystem();
	if (!Ship || !Inventory || !Weapons)
	{
		return;
	}

	TArray<UWeaponMountComponent*> Mounts;
	Ship->GetComponents(Mounts);
	for (const FStartechMountWeapon& Entry : Save.Mounts)
	{
		UWeaponMountComponent* const* const Mount = Mounts.FindByPredicate([&Entry](const UWeaponMountComponent* Each)
		{
			return Each->GetFName() == Entry.MountName;
		});
		if (!Mount)
		{
			UE_LOG(LogStartechSave, Warning, TEXT("%s: the ship has no mount '%s'; skipped."), Context, *Entry.MountName.ToString());
			continue;
		}
		UWeaponData* const Weapon = FStartechSaveConversion::Resolve<UWeaponData>(Entry.WeaponTag, Context);
		if (Entry.WeaponTag.IsValid() && !Weapon)
		{
			continue; // Unknown weapon: keep what the mount has rather than emptying it.
		}
		if (!FShipFitting::RestoreWeapon(*Ship, **Mount, Weapon))
		{
			UE_LOG(LogStartechSave, Warning, TEXT("%s: mount '%s' can't carry %s; skipped."), Context,
				*Entry.MountName.ToString(), *Entry.WeaponTag.ToString());
		}
	}

	Inventory->RestoreWeapons(FStartechSaveConversion::ToWeaponStacks(Save.SpareWeapons, Context));
	Weapons->RestoreAmmo(FStartechSaveConversion::ToAmmoStacks(Save.Ammo, Context));
	// Version 1 has no GroupSelections: SelectedAmmo restores its own group and the others take their first held ammo.
	TArray<UAmmoData*> PerGroup;
	for (const FGameplayTag& Tag : Save.GroupSelections)
	{
		if (UAmmoData* const Ammo = FStartechSaveConversion::Resolve<UAmmoData>(Tag, Context))
		{
			PerGroup.Add(Ammo);
		}
	}
	Weapons->RestoreSelections(PerGroup, FStartechSaveConversion::Resolve<UAmmoData>(Save.SelectedAmmo, Context));

	UE_LOG(LogStartechSave, Log, TEXT("%s: %d mount(s), %d spare stack(s), %d ammo type(s), active %s, %d group selection(s)."),
		Context, Save.Mounts.Num(), Inventory->GetWeapons().Num(), Weapons->GetAmmoStacks().Num(), *Save.SelectedAmmo.ToString(),
		Save.GroupSelections.Num());
}
