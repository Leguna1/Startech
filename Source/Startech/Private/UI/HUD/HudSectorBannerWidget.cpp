#include "UI/HUD/HudSectorBannerWidget.h"

#include "Core/SectorDirectorSubsystem.h"
#include "Data/SectorCatalog.h"
#include "Engine/World.h"
#include "Station.h"
#include "StartechShipPawn.h"
#include "Tiers/StartechTierSubsystem.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace SectorBanner
{
	// Fraction of the banner time spent cross-fading into the resting layout.
	constexpr float SettleFraction = 0.2f;
	constexpr float LineGap = 2.f;
}

UHudSectorBannerWidget::UHudSectorBannerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BootOrder = 4;
}

void UHudSectorBannerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UWorld* const World = GetWorld();
	USectorDirectorSubsystem* const Director = World ? World->GetSubsystem<USectorDirectorSubsystem>() : nullptr;
	BoundDirector = Director;
	if (Director)
	{
		Director->OnSectorReady.AddUniqueDynamic(this, &UHudSectorBannerWidget::HandleSectorReady);
	}

	// Again: the sync in Super ran before the director was known.
	SyncToPawn();
}

void UHudSectorBannerWidget::NativeDestruct()
{
	if (USectorDirectorSubsystem* const Director = BoundDirector.Get())
	{
		Director->OnSectorReady.RemoveDynamic(this, &UHudSectorBannerWidget::HandleSectorReady);
	}
	HandlePawnChanged(GetBoundPawn(), nullptr);

	Super::NativeDestruct();
}

void UHudSectorBannerWidget::HandlePawnChanged(APawn* const OldPawn, APawn* const NewPawn)
{
	if (AStartechShipPawn* const OldShip = Cast<AStartechShipPawn>(OldPawn))
	{
		OldShip->OnDockedChanged.RemoveDynamic(this, &UHudSectorBannerWidget::HandleDockedChanged);
	}

	Super::HandlePawnChanged(OldPawn, NewPawn);

	AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(NewPawn);
	if (Ship)
	{
		Ship->OnDockedChanged.AddUniqueDynamic(this, &UHudSectorBannerWidget::HandleDockedChanged);
	}
}

void UHudSectorBannerWidget::RefreshFromPawn()
{
	const USectorDirectorSubsystem* const Director = BoundDirector.Get();
	RefreshSector(Director ? Director->GetCurrentSectorId() : NAME_None);

	const AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(GetBoundPawn());
	const AStation* const Station = Ship ? Ship->GetDockedStation() : nullptr;
	bDocked = Station != nullptr;
	StationName = Station ? Station->GetDisplayName() : FText::GetEmpty();
	RefreshDockPrompt();
}

void UHudSectorBannerWidget::RefreshDockPrompt()
{
	const AStartechShipPawn* const Ship = Cast<AStartechShipPawn>(GetBoundPawn());
	const FText Key = Ship && bDocked ? Ship->GetInteractKeyText() : FText::GetEmpty();
	DockPrompt = Key.IsEmpty() ? FText::GetEmpty()
		: FText::Format(NSLOCTEXT("HudSectorBanner", "DockPrompt", "Press {0} to open {1} station"), Key, StationName);
}

void UHudSectorBannerWidget::HandleSectorReady(const FName SectorId)
{
	RefreshSector(SectorId);

	const USectorDirectorSubsystem* const Director = BoundDirector.Get();
	const bool bAfterWarp = Director && !Director->GetPreviousSectorId().IsNone();
	if (bAfterWarp)
	{
		// Mid-warp the HUD is folded away: the banner waits for this element's boot. Otherwise it plays now.
		if (GetFoldAmount() > 0.f)
		{
			bBannerPending = true;
		}
		else
		{
			PlayBanner();
		}
	}
	RequestRepaint();
}

void UHudSectorBannerWidget::HandleBootStarted()
{
	if (bBannerPending)
	{
		bBannerPending = false;
		PlayBanner();
	}
}

void UHudSectorBannerWidget::HandleDockedChanged(const bool bInDocked, const FText InStationName)
{
	bDocked = bInDocked;
	StationName = InStationName;
	RefreshDockPrompt();
	RequestRepaint();
}

void UHudSectorBannerWidget::RefreshSector(const FName SectorId)
{
	USectorDirectorSubsystem* const Director = BoundDirector.Get();
	const USectorCatalog* const Catalog = Director ? Director->GetCatalog() : nullptr;
	if (!Catalog || SectorId.IsNone())
	{
		SectorName = FText::GetEmpty();
		SectorLine = FText::GetEmpty();
		TierText = FText::GetEmpty();
		return;
	}

	const int32 Index = Catalog->Sectors.IndexOfByPredicate([SectorId](const FSectorDefinition& Each)
	{
		return Each.SectorId == SectorId;
	});
	const FSectorDefinition* const Definition = Catalog->Sectors.IsValidIndex(Index) ? &Catalog->Sectors[Index] : nullptr;
	const int32 Gates = Definition ? Definition->Gates.Num() : 0;

	SectorName = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName : FText::FromName(SectorId);
	// Read by sector id, so it doesn't depend on the tier subsystem having handled OnSectorReady first.
	if (const UStartechTierSubsystem* const Tiers = UStartechTierSubsystem::Get(this))
	{
		const FSectorTierData& Tier = Tiers->GetTierData(Tiers->GetTierForSector(SectorId));
		TierText = FText::Format(NSLOCTEXT("HudSectorBanner", "Tier", "Tier {0}"), Tier.DisplayName);
		TierColor = Tier.DangerColor;
	}
	else
	{
		TierText = FText::GetEmpty();
	}

	SectorLine = FText::Format(NSLOCTEXT("HudSectorBanner", "SectorLine", "Sector {0} · {1} {1}|plural(one=gate,other=gates)"),
		FText::AsNumber(Index + 1), FText::AsNumber(Gates));
}

void UHudSectorBannerWidget::PlayBanner()
{
	BannerStart = GetHudTime();
	RequestRepaint();
}

bool UHudSectorBannerWidget::IsBannerActive() const
{
	return BannerStart >= 0.0 && GetHudTime() - BannerStart < GetHudStyle().SectorBannerTime;
}

void UHudSectorBannerWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (BannerStart >= 0.0)
	{
		RequestRepaint();
		if (!IsBannerActive())
		{
			BannerStart = -1.0;
		}
	}
}

int32 UHudSectorBannerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);
	const int32 TextLayer = BaseLayer + 1;

	const UStartechHudStyle& HudStyle = GetHudStyle();
	const float CentreX = AllottedGeometry.GetLocalSize().X * 0.5f;
	const FSlateFontInfo SmallFont = UStartechHudStyle::MakeFont(HudStyle.SmallFontSize);

	// The large banner shows, then cross-fades into the resting layout over the last part of its time.
	float BannerAlpha = 0.f;
	if (IsBannerActive())
	{
		const float Elapsed = static_cast<float>(GetHudTime() - BannerStart) / FMath::Max(HudStyle.SectorBannerTime, 0.01f);
		BannerAlpha = 1.f - FHudDraw::Ease((Elapsed - (1.f - SectorBanner::SettleFraction)) / SectorBanner::SettleFraction);
	}
	const float RestAlpha = 1.f - BannerAlpha;

	// "Rust · " in the Player colour, then "Tier II" in the tier's colour.
	const FString TierString = TierText.ToString();
	const FString NameWithSeparator = TierString.IsEmpty() ? SectorName.ToString() : SectorName.ToString() + TEXT(" · ");
	const FLinearColor TierLinear = HudStyle.GetTierColor(TierColor);

	if (BannerAlpha > 0.f)
	{
		const FVector2f NameSize = FHudDraw::TwoColourText(OutDrawElements, TextLayer, AllottedGeometry, NameWithSeparator,
			FHudDraw::WithOpacity(HudStyle.PlayerColor, BannerAlpha), TierString,
			FHudDraw::WithOpacity(TierLinear, BannerAlpha), UStartechHudStyle::MakeFont(HudStyle.BannerFontSize, true),
			FVector2f(CentreX, 0.f), EHudTextAlign::Centre);
		FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, SectorLine.ToString(), SmallFont,
			FVector2f(CentreX, NameSize.Y + SectorBanner::LineGap),
			FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, BannerAlpha), EHudTextAlign::Centre);
	}

	if (RestAlpha > 0.f)
	{
		float Y = 0.f;
		Y += FHudDraw::TwoColourText(OutDrawElements, TextLayer, AllottedGeometry, NameWithSeparator,
			FHudDraw::WithOpacity(HudStyle.PlayerColor, RestAlpha), TierString,
			FHudDraw::WithOpacity(TierLinear, RestAlpha), UStartechHudStyle::MakeFont(HudStyle.HeadingFontSize, true),
			FVector2f(CentreX, Y), EHudTextAlign::Centre).Y + SectorBanner::LineGap;
		Y += FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, SectorLine.ToString(), SmallFont,
			FVector2f(CentreX, Y), FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, RestAlpha), EHudTextAlign::Centre).Y
			+ SectorBanner::LineGap;

		if (bDocked)
		{
			const FText Docked = FText::Format(NSLOCTEXT("HudSectorBanner", "Docked", "Docked · {0}"), StationName);
			Y += FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, Docked.ToString(), SmallFont, FVector2f(CentreX, Y),
				FHudDraw::WithOpacity(HudStyle.PlayerColor, RestAlpha), EHudTextAlign::Centre).Y + SectorBanner::LineGap;

			if (!DockPrompt.IsEmpty())
			{
				FHudDraw::Text(OutDrawElements, TextLayer, AllottedGeometry, DockPrompt.ToString(), SmallFont,
					FVector2f(CentreX, Y), FHudDraw::WithOpacity(HudStyle.TextSecondaryColor, RestAlpha), EHudTextAlign::Centre);
			}
		}
	}

	return TextLayer;
}
