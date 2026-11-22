// Console commands that drive every HUD state from real gameplay code paths, so the HUD's events can be checked in the
// Output Log (category LogStartechHUD) before any widget exists. Not compiled into shipping builds.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "InventoryComponent.h"
#include "ItemData.h"
#include "PlayerProfileSubsystem.h"
#include "StartechHUDLog.h"
#include "StartechShipPawn.h"
#include "UObject/Package.h"

namespace StartechHUDCommands
{
	/** How far ahead of the ship Startech.HUD.Damage says the hit came from, in cm. */
	constexpr float DamageSourceDistance = 1000.f;

	/** Object name of the transient item Startech.HUD.Cargo adds. */
	const TCHAR* const TestItemName = TEXT("HUDTestItem");

	/** The local player's ship, or null with a log line saying why. */
	AStartechShipPawn* GetPlayerShip(const UWorld* const World)
	{
		const APlayerController* const PC = World ? World->GetFirstPlayerController() : nullptr;
		AStartechShipPawn* const Ship = PC ? Cast<AStartechShipPawn>(PC->GetPawn()) : nullptr;
		if (!Ship)
		{
			UE_LOG(LogStartechHUD, Warning, TEXT("No player ship in this world; start Play In Editor first."));
		}
		return Ship;
	}

	/** One transient item asset, made on first use and reused so the cargo stacks. */
	UItemData* GetTestItem()
	{
		if (UItemData* const Existing = FindObject<UItemData>(GetTransientPackage(), TestItemName))
		{
			return Existing;
		}

		UItemData* const Item = NewObject<UItemData>(GetTransientPackage(), TestItemName, RF_Transient);
		Item->DisplayName = NSLOCTEXT("StartechHUD", "TestItem", "Test Ore");
		return Item;
	}

	/** The argument at Index as a float, or Default when it is missing. */
	float ArgFloat(const TArray<FString>& Args, const int32 Index, const float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	/** The argument at Index as an integer, or Default when it is missing. */
	int32 ArgInt(const TArray<FString>& Args, const int32 Index, const int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	/** Startech.HUD.Damage <shield> <hull> */
	void Damage(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipPawn* const Ship = GetPlayerShip(World);
		UHealthComponent* const Health = Ship ? Ship->GetHealthComponent() : nullptr;
		if (!Health)
		{
			return;
		}

		const float ShieldAmount = ArgFloat(Args, 0, 0.f);
		const float HullAmount = ArgFloat(Args, 1, 0.f);
		const FVector Source = Ship->GetActorLocation() + Ship->GetActorForwardVector() * DamageSourceDistance;

		UE_LOG(LogStartechHUD, Display, TEXT("Startech.HUD.Damage: shield %.1f, hull %.1f from %s"),
			ShieldAmount, HullAmount, *Source.ToCompactString());
		Health->ApplyDirectDamage(ShieldAmount, HullAmount, Source);
	}

	/** Startech.HUD.Cargo <amount> */
	void Cargo(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipPawn* const Ship = GetPlayerShip(World);
		UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr;
		if (!Inventory)
		{
			return;
		}

		UItemData* const Item = GetTestItem();
		const int32 Amount = ArgInt(Args, 0, 1);
		if (Amount >= 0)
		{
			const int32 Added = Inventory->AddItem(Item, Amount);
			UE_LOG(LogStartechHUD, Display, TEXT("Startech.HUD.Cargo: added %d of %d (hold %d / %d)"),
				Added, Amount, Inventory->GetCargoUsed(), Inventory->GetCargoCapacity());
		}
		else
		{
			const int32 Removed = FMath::Min(-Amount, Inventory->GetItemCount(Item));
			Inventory->RemoveItem(Item, Removed);
			UE_LOG(LogStartechHUD, Display, TEXT("Startech.HUD.Cargo: removed %d test items (hold %d / %d)"),
				Removed, Inventory->GetCargoUsed(), Inventory->GetCargoCapacity());
		}
	}

	/** Startech.HUD.Credits <amount> */
	void Credits(const TArray<FString>& Args, UWorld* const World)
	{
		const AStartechShipPawn* const Ship = GetPlayerShip(World);
		UInventoryComponent* const Inventory = Ship ? Ship->GetInventory() : nullptr;
		if (!Inventory)
		{
			return;
		}

		const int32 Amount = ArgInt(Args, 0, 0);
		if (Amount >= 0)
		{
			Inventory->AddCredits(Amount);
		}
		else
		{
			// Spending more than is held would be refused; take what there is instead.
			Inventory->SpendCredits(FMath::Min(-Amount, Inventory->GetCredits()));
		}
		UE_LOG(LogStartechHUD, Display, TEXT("Startech.HUD.Credits: %+d, now %d"), Amount, Inventory->GetCredits());
	}

	/** Startech.HUD.Callsign <name> */
	void Callsign(const TArray<FString>& Args, UWorld* const World)
	{
		UPlayerProfileSubsystem* const Profile = UPlayerProfileSubsystem::Get(World);
		if (!Profile)
		{
			UE_LOG(LogStartechHUD, Warning, TEXT("Startech.HUD.Callsign: no game instance."));
			return;
		}

		Profile->SetCallsign(FText::FromString(FString::Join(Args, TEXT(" "))));
		UE_LOG(LogStartechHUD, Display, TEXT("Startech.HUD.Callsign: callsign '%s', ship id %s"),
			*Profile->GetCallsign().ToString(), *Profile->GetShipId().ToString());
	}

	/** Startech.HUD.OutOfBounds <0/1> */
	void OutOfBounds(const TArray<FString>& Args, UWorld* const World)
	{
		AStartechShipPawn* const Ship = GetPlayerShip(World);
		if (!Ship)
		{
			return;
		}

		const bool bForce = ArgInt(Args, 0, 1) != 0;
		Ship->SetForceOutOfBounds(bForce);
		UE_LOG(LogStartechHUD, Display, TEXT("Startech.HUD.OutOfBounds: forced %s (takes effect next frame)"),
			bForce ? TEXT("on") : TEXT("off"));
	}

	FAutoConsoleCommandWithWorldAndArgs DamageCommand(TEXT("Startech.HUD.Damage"),
		TEXT("Startech.HUD.Damage <shield> <hull>: damages the player's shield and hull by those amounts, from 1000 units ahead."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Damage));

	FAutoConsoleCommandWithWorldAndArgs CargoCommand(TEXT("Startech.HUD.Cargo"),
		TEXT("Startech.HUD.Cargo <amount>: adds that many units of a test item to the player's cargo (negative removes)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Cargo));

	FAutoConsoleCommandWithWorldAndArgs CreditsCommand(TEXT("Startech.HUD.Credits"),
		TEXT("Startech.HUD.Credits <amount>: adds credits, or removes them when negative."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Credits));

	FAutoConsoleCommandWithWorldAndArgs CallsignCommand(TEXT("Startech.HUD.Callsign"),
		TEXT("Startech.HUD.Callsign <name>: sets the player's callsign (empty restores \"Captain\")."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Callsign));

	FAutoConsoleCommandWithWorldAndArgs OutOfBoundsCommand(TEXT("Startech.HUD.OutOfBounds"),
		TEXT("Startech.HUD.OutOfBounds <0/1>: forces the out-of-bounds state on or off, with the real countdown."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&OutOfBounds));
}

#endif // !UE_BUILD_SHIPPING
