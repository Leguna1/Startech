#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "ActionBar/ActionBarComponent.h"
#include "AmmoData.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "StartechSettings.h"
#include "StartechShipPawn.h"
#include "UI/HUD/StartechHudStyle.h"
#include "WeaponSystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechActionBar, Log, All);

/**
 * Action bar debug commands (non-shipping):
 *   Startech.Test.ActionBar              logs the active group and every slot: key, item, selected, count and flags
 *   Startech.Test.SetAmmo <Tag> <N>      sets an ammo count, for the low and empty states
 *   Startech.Test.InfiniteAmmo <0|1>     switches infinite ammo (the bar shows the infinity sign)
 *   Startech.Test.ActionBarFakeTimers 1  (console variable) fakes a cooldown and an active effect on the last slots
 */
namespace StartechActionBarConsole
{
	/** The player's ship in World, or null. */
	static AStartechShipPawn* GetShip(const UWorld* const World)
	{
		return World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	}

	static void Dump(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipPawn* const Ship = GetShip(World);
		const UActionBarComponent* const Bar = Ship ? Ship->GetActionBar() : nullptr;
		if (!Bar)
		{
			UE_LOG(LogStartechActionBar, Warning, TEXT("No player ship with an action bar."));
			return;
		}

		const UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem();
		const UAmmoData* const Active = Weapons->GetSelectedAmmo();
		UE_LOG(LogStartechActionBar, Display, TEXT("[Bar] active group %s, firing %s"),
			*StaticEnum<EWeaponGroup>()->GetNameStringByValue(static_cast<int64>(Weapons->GetActiveGroup())),
			Active ? *Active->ItemTag.ToString() : TEXT("nothing"));

		const int32 LowThreshold = UStartechHudStyle::GetDefaultStyle().LowAmmoThreshold;
		for (int32 Index = 0; Index < Bar->GetSlotCount(); ++Index)
		{
			const FActionBarSlot& Slot = Bar->GetSlot(Index);
			const FActionSlotState State = Bar->GetSlotState(Index, LowThreshold);
			UE_LOG(LogStartechActionBar, Display, TEXT("[Bar] key %s %-14s %-8s %s count=%d%s%s%s%s%s"),
				*Bar->GetSlotKeyText(Index).ToString(), *Slot.ItemTag.ToString(), *Slot.GroupLabel.ToString(),
				State.bSelected ? TEXT("SELECTED") : TEXT("--------"), State.Count,
				State.bInActiveGroup ? TEXT(" active-group") : TEXT(""), State.bLow ? TEXT(" low") : TEXT(""),
				State.bEmpty ? TEXT(" empty") : TEXT(""), State.bOutOfRange ? TEXT(" OUT-OF-RANGE") : TEXT(""),
				State.bDisabled ? TEXT(" disabled") : TEXT(""));
		}
	}

	static void SetAmmo(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipPawn* const Ship = GetShip(World);
		UAmmoData* const Ammo = Args.Num() >= 2
			? UStartechSettings::Get().FindItemOfType<UAmmoData>(FGameplayTag::RequestGameplayTag(FName(*Args[0]), false)) : nullptr;
		if (!Ship || !Ammo)
		{
			UE_LOG(LogStartechActionBar, Warning, TEXT("Usage: Startech.Test.SetAmmo <AmmoTag> <Count>"));
			return;
		}
		UWeaponSystemComponent* const Weapons = Ship->GetWeaponSystem();
		const float Target = FMath::Max(FCString::Atof(*Args[1]), 0.f);
		const float Current = Weapons->GetAmmoCount(Ammo);
		if (Target > Current)
		{
			Weapons->AddAmmo(Ammo, Target - Current);
		}
		else if (Target < Current)
		{
			Weapons->ConsumeAmmo(Ammo, Current - Target);
		}
	}

	static void InfiniteAmmo(const TArray<FString>& Args, UWorld* const World)
	{
		if (AStartechShipPawn* const Ship = GetShip(World))
		{
			Ship->GetWeaponSystem()->SetInfiniteAmmo(!Args.IsEmpty() && FCString::Atoi(*Args[0]) != 0);
			// Infinite ammo has no event of its own; tell the bar.
			Ship->GetActionBar()->OnSlotsChanged.Broadcast();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(TEXT("Startech.Test.ActionBar"),
		TEXT("Logs the active weapon group and every action bar slot's state."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Dump));

	static FAutoConsoleCommandWithWorldAndArgs SetAmmoCommand(TEXT("Startech.Test.SetAmmo"),
		TEXT("Startech.Test.SetAmmo <AmmoTag> <Count>: sets an ammo count (adds or uses the difference)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetAmmo));

	static FAutoConsoleCommandWithWorldAndArgs InfiniteAmmoCommand(TEXT("Startech.Test.InfiniteAmmo"),
		TEXT("Startech.Test.InfiniteAmmo <0|1>: switches the player's infinite ammo (the bar shows the infinity sign)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&InfiniteAmmo));
}

#endif
