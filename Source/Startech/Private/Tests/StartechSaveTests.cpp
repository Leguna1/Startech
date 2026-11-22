#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Save/StartechSaveTypes.h"
#include "SaveSerialization.h"
#include "StartechGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartechSaveGameplayTagsTest, "Startech.Save.GameplayTags",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

/** Cargo, ammo and weapons are stored by gameplay tag: tags must survive SaveFramework's serialization. */
bool FStartechSaveGameplayTagsTest::RunTest(const FString& Parameters)
{
	FStartechArsenalSave Arsenal;
	FStartechTaggedAmount& Ammo = Arsenal.Ammo.AddDefaulted_GetRef();
	Ammo.ItemTag = StartechTags::Item_Ammo_LL2;
	Ammo.Amount = 42.5f;
	FStartechMountWeapon& Mount = Arsenal.Mounts.AddDefaulted_GetRef();
	Mount.MountName = TEXT("LaserMountLeft");
	Mount.WeaponTag = StartechTags::Item_Weapon_Laser;
	FStartechTaggedCount& Spare = Arsenal.SpareWeapons.AddDefaulted_GetRef();
	Spare.ItemTag = StartechTags::Item_Weapon_ShieldDrainer;
	Spare.Count = 2;
	Arsenal.SelectedAmmo = StartechTags::Item_Ammo_SD1;

	FStartechCargoSave Cargo;
	FStartechTaggedCount& Mineral = Cargo.Items.AddDefaulted_GetRef();
	Mineral.ItemTag = StartechTags::Item_Mineral_Aurium;
	Mineral.Count = 17;

	TArray<uint8> ArsenalBytes;
	TArray<uint8> CargoBytes;
	FSaveSerialization::StructToBytes(Arsenal, ArsenalBytes);
	FSaveSerialization::StructToBytes(Cargo, CargoBytes);

	FStartechArsenalSave ArsenalBack;
	FStartechCargoSave CargoBack;
	TestTrue(TEXT("Arsenal read back"), FSaveSerialization::BytesToStruct(ArsenalBytes, ArsenalBack));
	TestTrue(TEXT("Cargo read back"), FSaveSerialization::BytesToStruct(CargoBytes, CargoBack));

	if (TestEqual(TEXT("Ammo entries"), ArsenalBack.Ammo.Num(), 1))
	{
		TestTrue(TEXT("Ammo tag survives"), ArsenalBack.Ammo[0].ItemTag.MatchesTagExact(StartechTags::Item_Ammo_LL2));
		TestEqual(TEXT("Ammo amount"), ArsenalBack.Ammo[0].Amount, 42.5f);
	}
	if (TestEqual(TEXT("Mount entries"), ArsenalBack.Mounts.Num(), 1))
	{
		TestEqual(TEXT("Mount name"), ArsenalBack.Mounts[0].MountName, FName(TEXT("LaserMountLeft")));
		TestTrue(TEXT("Weapon tag survives"), ArsenalBack.Mounts[0].WeaponTag.MatchesTagExact(StartechTags::Item_Weapon_Laser));
	}
	if (TestEqual(TEXT("Spare entries"), ArsenalBack.SpareWeapons.Num(), 1))
	{
		TestTrue(TEXT("Spare tag survives"), ArsenalBack.SpareWeapons[0].ItemTag.MatchesTagExact(StartechTags::Item_Weapon_ShieldDrainer));
		TestEqual(TEXT("Spare count"), ArsenalBack.SpareWeapons[0].Count, 2);
	}
	TestTrue(TEXT("Selected ammo tag survives"), ArsenalBack.SelectedAmmo.MatchesTagExact(StartechTags::Item_Ammo_SD1));
	if (TestEqual(TEXT("Cargo entries"), CargoBack.Items.Num(), 1))
	{
		TestTrue(TEXT("Mineral tag survives"), CargoBack.Items[0].ItemTag.MatchesTagExact(StartechTags::Item_Mineral_Aurium));
		TestEqual(TEXT("Mineral count"), CargoBack.Items[0].Count, 17);
	}

	// A container too, the way enemy tags are held, in case a later participant stores one.
	FGameplayTagContainer Container;
	Container.AddTag(StartechTags::Enemy_Drifter);
	Container.AddTag(StartechTags::Item_Ammo_LL4);
	TArray<uint8> ContainerBytes;
	FSaveSerialization::StructToBytes(Container, ContainerBytes);
	FGameplayTagContainer ContainerBack;
	TestTrue(TEXT("Container read back"), FSaveSerialization::BytesToStruct(ContainerBytes, ContainerBack));
	TestTrue(TEXT("Container tags survive"), ContainerBack.HasTagExact(StartechTags::Enemy_Drifter)
		&& ContainerBack.HasTagExact(StartechTags::Item_Ammo_LL4) && ContainerBack.Num() == 2);
	return true;
}

#endif
