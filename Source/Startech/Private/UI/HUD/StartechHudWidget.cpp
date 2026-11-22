#include "UI/HUD/StartechHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gates/WarpTransitionComponent.h"
#include "HomeRespawnSubsystem.h"
#include "Misc/App.h"
#include "StartechHUDLog.h"
#include "StartechShipPawn.h"
#include "UI/HUD/HudElementWidget.h"
#include "UI/HUD/StartechHudStyle.h"

void UStartechHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GatherElements();

	if (APlayerController* const PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UStartechHudWidget::HandlePossessedPawnChanged);
		BoundController = PC;
	}
	SetPawn(GetOwningPlayerPawn());

	UWorld* const World = GetWorld();
	if (UHomeRespawnSubsystem* const Respawn = World ? World->GetSubsystem<UHomeRespawnSubsystem>() : nullptr)
	{
		Respawn->OnRespawnSequenceChanged.AddUniqueDynamic(this, &UStartechHudWidget::HandleRespawnSequenceChanged);
		BoundRespawn = Respawn;
	}
}

void UStartechHudWidget::NativeDestruct()
{
	if (AController* const Controller = BoundController.Get())
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &UStartechHudWidget::HandlePossessedPawnChanged);
	}
	BoundController.Reset();
	if (UHomeRespawnSubsystem* const Respawn = BoundRespawn.Get())
	{
		Respawn->OnRespawnSequenceChanged.RemoveDynamic(this, &UStartechHudWidget::HandleRespawnSequenceChanged);
	}
	BoundRespawn.Reset();
	SetPawn(nullptr);

	Super::NativeDestruct();
}

void UStartechHudWidget::GatherElements()
{
	Elements.Reset();
	if (!WidgetTree)
	{
		return;
	}

	const UStartechHudStyle* const HudStyle = Style ? Style.Get() : &UStartechHudStyle::GetDefaultStyle();
	WidgetTree->ForEachWidget([this, HudStyle](UWidget* const Widget)
	{
		if (UHudElementWidget* const Element = Cast<UHudElementWidget>(Widget))
		{
			Element->SetInheritedStyle(HudStyle);
			FElementAnim& Anim = Elements.AddDefaulted_GetRef();
			Anim.Element = Element;
		}
	});

	STARTECH_HUD_LOG(TEXT("HUD root found %d element(s)"), Elements.Num());
}

void UStartechHudWidget::HandlePossessedPawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	SetPawn(NewPawn);
}

void UStartechHudWidget::SetPawn(APawn* const NewPawn)
{
	APawn* const OldPawn = BoundPawn.Get();
	if (OldPawn == NewPawn && (NewPawn || !BoundWarp.IsValid()))
	{
		return;
	}

	if (UWarpTransitionComponent* const Warp = BoundWarp.Get())
	{
		Warp->OnWarpStarted.RemoveDynamic(this, &UStartechHudWidget::HandleWarpStarted);
		Warp->OnWarpFinished.RemoveDynamic(this, &UStartechHudWidget::HandleWarpFinished);
	}
	BoundWarp.Reset();
	if (AStartechShipPawn* const Ship = BoundShip.Get())
	{
		Ship->OnPlayerRespawned.RemoveDynamic(this, &UStartechHudWidget::HandlePlayerRespawned);
		Ship->OnHubOpenChanged.RemoveDynamic(this, &UStartechHudWidget::HandleHubOpenChanged);
	}
	BoundShip.Reset();
	BoundPawn = NewPawn;

	// Each element binds, then shows the new pawn's current values at once.
	for (const FElementAnim& Anim : Elements)
	{
		if (UHudElementWidget* const Element = Anim.Element.Get())
		{
			Element->BindToPawn(OldPawn, NewPawn);
		}
	}

	if (AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn))
	{
		Ship->OnPlayerRespawned.AddUniqueDynamic(this, &UStartechHudWidget::HandlePlayerRespawned);
		Ship->OnHubOpenChanged.AddUniqueDynamic(this, &UStartechHudWidget::HandleHubOpenChanged);
		BoundShip = Ship;
	}
	const AStartechShipPawn* const NewShip = BoundShip.Get();
	SetHiddenForHub(NewShip && NewShip->IsHubOpen());

	// The warp component isn't referenced in C++; BP_Ship adds it.
	UWarpTransitionComponent* const Warp = NewPawn ? NewPawn->FindComponentByClass<UWarpTransitionComponent>() : nullptr;
	if (Warp)
	{
		Warp->OnWarpStarted.AddUniqueDynamic(this, &UStartechHudWidget::HandleWarpStarted);
		Warp->OnWarpFinished.AddUniqueDynamic(this, &UStartechHudWidget::HandleWarpFinished);
		BoundWarp = Warp;
	}
	else if (NewPawn)
	{
		UE_LOG(LogStartechHUD, Warning, TEXT("%s has no UWarpTransitionComponent; the HUD won't fold on warps."),
			*NewPawn->GetName());
	}
}

void UStartechHudWidget::HandleWarpStarted(const FName DestinationSectorId)
{
	StartFold();
}

void UStartechHudWidget::HandleWarpFinished(const FName SectorId)
{
	// Fresh values before the elements boot back in. A warp that ends while the player is dead leaves the HUD
	// folded: the respawn boots it.
	SyncElements();
	if (!IsRespawning())
	{
		StartBoot();
	}
}

void UStartechHudWidget::HandleRespawnSequenceChanged(const bool bActive)
{
	if (bActive)
	{
		StartFold(true);
	}
	else
	{
		SyncElements();
		StartBoot();
	}
}

bool UStartechHudWidget::IsRespawning() const
{
	const UHomeRespawnSubsystem* const Respawn = BoundRespawn.Get();
	return Respawn && Respawn->IsRespawning();
}

void UStartechHudWidget::HandleHubOpenChanged(const bool bOpen)
{
	SetHiddenForHub(bOpen);
}

void UStartechHudWidget::SetHiddenForHub(const bool bHidden)
{
	if (bHidden == bHiddenForHub)
	{
		return;
	}
	bHiddenForHub = bHidden;

	if (bHidden)
	{
		VisibilityBeforeHub = GetVisibility();
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SetVisibility(VisibilityBeforeHub);
	}
}

void UStartechHudWidget::HandlePlayerRespawned()
{
	SyncElements();
}

void UStartechHudWidget::SyncElements()
{
	for (const FElementAnim& Anim : Elements)
	{
		if (UHudElementWidget* const Element = Anim.Element.Get())
		{
			Element->SyncToPawn();
		}
	}
}

void UStartechHudWidget::StartFold(const bool bForDeath)
{
	const UStartechHudStyle& HudStyle = Style ? *Style : UStartechHudStyle::GetDefaultStyle();
	const double Now = FApp::GetCurrentTime();
	for (FElementAnim& Anim : Elements)
	{
		const UHudElementWidget* const Element = Anim.Element.Get();
		Anim.From = Element ? Element->GetFoldAmount() : 0.f;
		Anim.To = bForDeath && Element && Element->StaysUpWhileRespawning() ? 0.f : 1.f;
		Anim.Start = Now;
		Anim.Duration = HudStyle.FoldTime;
		Anim.bActive = true;
		Anim.bBootNotified = true;
	}
}

void UStartechHudWidget::StartBoot()
{
	const UStartechHudStyle& HudStyle = Style ? *Style : UStartechHudStyle::GetDefaultStyle();
	const double Now = FApp::GetCurrentTime();

	// Each distinct boot order is one step of the stagger, so elements sharing an order boot together.
	TArray<int32> Orders;
	for (const FElementAnim& Anim : Elements)
	{
		if (const UHudElementWidget* const Element = Anim.Element.Get())
		{
			Orders.AddUnique(Element->GetBootOrder());
		}
	}
	Orders.Sort();

	for (FElementAnim& Anim : Elements)
	{
		const UHudElementWidget* const Element = Anim.Element.Get();
		if (!Element)
		{
			continue;
		}
		const int32 Step = Orders.IndexOfByKey(Element->GetBootOrder());
		Anim.From = Element->GetFoldAmount();
		Anim.To = 0.f;
		Anim.Start = Now + Step * HudStyle.BootStagger;
		Anim.Duration = HudStyle.BootTime;
		Anim.bActive = true;
		Anim.bBootNotified = false;
	}
}

void UStartechHudWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const double Now = FApp::GetCurrentTime();
	for (FElementAnim& Anim : Elements)
	{
		UHudElementWidget* const Element = Anim.Element.Get();
		if (!Anim.bActive || !Element || Now < Anim.Start)
		{
			continue;
		}

		if (!Anim.bBootNotified)
		{
			Anim.bBootNotified = true;
			Element->HandleBootStarted();
		}

		const float Alpha = Anim.Duration > 0.f ? FMath::Clamp(static_cast<float>(Now - Anim.Start) / Anim.Duration, 0.f, 1.f) : 1.f;
		Element->SetFoldAmount(FMath::Lerp(Anim.From, Anim.To, Alpha));
		Anim.bActive = Alpha < 1.f;
	}
}
