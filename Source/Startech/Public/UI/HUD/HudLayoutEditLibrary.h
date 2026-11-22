#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HudLayoutEditLibrary.generated.h"

/**
 * Editor-only edits of the HUD's Widget Blueprint (WB_GameHUD), for the layout scripts in Tools/HUD. Python can't
 * reach a Widget Blueprint's widget tree, so the scripts call these. The caller saves the asset afterwards.
 */
UCLASS()
class STARTECH_API UHudLayoutEditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * One line per widget in WidgetBlueprint's designer tree: depth, name, class, and for widgets in a canvas their
	 * anchors, alignment, position and size. Empty when WidgetBlueprint is not a Widget Blueprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Editor")
	static TArray<FString> DescribeWidgetTree(UObject* WidgetBlueprint);

	/**
	 * The action bar task's layout, idempotent. (Its first run, on 2026-11-21, also removed the weapon strip and the
	 * old action slot widgets; those classes are gone now.)
	 * - adds an Invalidation Box ("IB_ActionBar") holding a UHudActionBarWidget ("HudActionBar"): anchors bottom centre,
	 *   alignment (0.5, 1), position (0, -28), size 376 x 96; an existing one is only re-placed;
	 * - places the UHudSpeedReadoutWidget's wrapper right of the bar: anchors bottom centre, alignment (0, 1),
	 *   position (204, -42), size 120 x 64.
	 * Then compiles the Blueprint. OutReport lists every change. False when nothing could be applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Editor")
	static bool ApplyActionBarLayout(UObject* WidgetBlueprint, TArray<FString>& OutReport);
#endif
};
