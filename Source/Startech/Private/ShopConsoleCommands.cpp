#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "ShopEntryWidget.h"
#include "ShopScreenWidget.h"
#include "TradeableData.h"
#include "UI/Hub/HubButtonWidget.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechShopConsole, Log, All);

/**
 * Shop debug commands (non-shipping):
 *   Startech.Test.ShopTab <Index>   selects a tab of the open shop screen (0.. the station's shops, then Loadout)
 *   Startech.Test.Shop              logs "[Shop]": the quantity, each row's Buy and Sell labels, and the status line
 *   Startech.Test.ClickShop <Row> <Buy|Sell> [Shift]
 *                                   a real left click (Slate mouse events at the button's centre), Shift held or not
 * Keys 1-5 pick the quantity: send them with Startech.Test.KeyDown One..Five (windowed runs).
 */
namespace StartechShopConsole
{
	/** The shop screen open in World, or null. */
	static UShopScreenWidget* FindScreen(const UWorld* const World)
	{
		for (TObjectIterator<UShopScreenWidget> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsConstructed() && !It->HasAnyFlags(RF_ClassDefaultObject))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static void ShopTab(const TArray<FString>& Args, UWorld* const World)
	{
		UShopScreenWidget* const Screen = FindScreen(World);
		if (!Screen || Args.IsEmpty())
		{
			UE_LOG(LogStartechShopConsole, Warning, TEXT("Usage: Startech.Test.ShopTab <Index>, with the hub open."));
			return;
		}
		Screen->SelectShop(FCString::Atoi(*Args[0]));
		UE_LOG(LogStartechShopConsole, Display, TEXT("[Shop] tab %s selected."), *Args[0]);
	}

	static void Shop(const TArray<FString>& Args, UWorld* const World)
	{
		const UShopScreenWidget* const Screen = FindScreen(World);
		if (!Screen)
		{
			UE_LOG(LogStartechShopConsole, Warning, TEXT("[Shop] No shop screen open."));
			return;
		}
		UE_LOG(LogStartechShopConsole, Display, TEXT("[Shop] quantity %s, status \"%s\""),
			*StaticEnum<EShopQuantity>()->GetNameStringByValue(static_cast<int64>(Screen->GetQuantity())),
			*Screen->GetStatus().ToString());
		const TArray<TObjectPtr<UShopEntryWidget>>& Rows = Screen->GetRows();
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const UShopEntryWidget* const Row = Rows[Index];
			const UHubButtonWidget* const BuyButton = Row->GetButton(ETradeDirection::Buy);
			const UHubButtonWidget* const SellButton = Row->GetButton(ETradeDirection::Sell);
			// A side the row doesn't offer (ammo is never bought back) is hidden, whatever its label.
			const auto State = [](const UHubButtonWidget& Button)
			{
				return !Button.IsVisible() ? TEXT(" (hidden)") : Button.IsButtonEnabled() ? TEXT("") : TEXT(" (disabled)");
			};
			UE_LOG(LogStartechShopConsole, Display, TEXT("[Shop] row %d %s: buy \"%s\"%s, sell \"%s\"%s"), Index,
				Row->GetGood() ? *Row->GetGood()->DisplayName.ToString() : TEXT("?"),
				*BuyButton->GetDisplayText(), State(*BuyButton), *SellButton->GetDisplayText(), State(*SellButton));
		}
	}

	static void ClickShop(const TArray<FString>& Args, UWorld* const World)
	{
		const UShopScreenWidget* const Screen = FindScreen(World);
		const int32 RowIndex = Args.Num() > 1 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
		if (!Screen || !Screen->GetRows().IsValidIndex(RowIndex) || !FSlateApplication::IsInitialized())
		{
			UE_LOG(LogStartechShopConsole, Warning, TEXT("Usage: Startech.Test.ClickShop <Row> <Buy|Sell> [Shift], with the shop open."));
			return;
		}

		const ETradeDirection Direction = Args[1].Equals(TEXT("Sell"), ESearchCase::IgnoreCase) ? ETradeDirection::Sell : ETradeDirection::Buy;
		const bool bShift = Args.Num() > 2 && Args[2].Equals(TEXT("Shift"), ESearchCase::IgnoreCase);
		const UHubButtonWidget* const Button = Screen->GetRows()[RowIndex]->GetButton(Direction);
		const FGeometry& Geometry = Button->GetCachedGeometry();
		const FVector2D Centre = FVector2D(Geometry.GetAbsolutePosition() + Geometry.GetAbsoluteSize() * 0.5f);

		FSlateApplication& Slate = FSlateApplication::Get();
		const FModifierKeysState Modifiers(bShift, false, false, false, false, false, false, false, false);
		const TSet<FKey> NoButtons;
		const TSet<FKey> LeftButton = { EKeys::LeftMouseButton };

		Slate.SetCursorPos(Centre);
		Slate.ProcessMouseMoveEvent(FPointerEvent(0, Centre, Centre, NoButtons, EKeys::Invalid, 0.f, Modifiers));
		Slate.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(0, Centre, Centre, LeftButton, EKeys::LeftMouseButton, 0.f, Modifiers));
		Slate.ProcessMouseButtonUpEvent(FPointerEvent(0, Centre, Centre, NoButtons, EKeys::LeftMouseButton, 0.f, Modifiers));

		UE_LOG(LogStartechShopConsole, Display, TEXT("[Shop] clicked row %d %s%s at %s; status \"%s\""), RowIndex,
			Direction == ETradeDirection::Buy ? TEXT("Buy") : TEXT("Sell"), bShift ? TEXT(" with Shift") : TEXT(""),
			*Centre.ToString(), *Screen->GetStatus().ToString());
	}

	static FAutoConsoleCommandWithWorldAndArgs ShopCommand(TEXT("Startech.Test.Shop"),
		TEXT("Logs the shop's quantity, every row's Buy and Sell labels and the status line."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Shop));

	static FAutoConsoleCommandWithWorldAndArgs ClickShopCommand(TEXT("Startech.Test.ClickShop"),
		TEXT("Startech.Test.ClickShop <Row> <Buy|Sell> [Shift]: a real mouse click on a shop row's button."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClickShop));

	static FAutoConsoleCommandWithWorldAndArgs ShopTabCommand(TEXT("Startech.Test.ShopTab"),
		TEXT("Startech.Test.ShopTab <Index>: selects a tab of the open shop screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShopTab));
}

#endif
