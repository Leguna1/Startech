#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "HudPanelBrush.generated.h"

class FSlateWindowElementList;
class UMaterialInstanceDynamic;
class UStartechHudStyle;
struct FGeometry;

/**
 * One cut-corner panel background for a HUD element: a dynamic instance of the style's M_HUD_ChamferPanel, whose Size
 * parameter is kept equal to the drawn size so its corners stay sharp. Without the material it draws a plain fill
 * with a cut-corner outline instead. Owned as a UPROPERTY by the widget that draws it.
 */
USTRUCT()
struct STARTECH_API FHudPanelBrush
{
	GENERATED_BODY()

	/**
	 * Makes the material instance from Style's panel material, with Outer as its owner, and sets its colours.
	 * Call again when the style changes. Safe without a material.
	 */
	void Initialize(UObject* Outer, const UStartechHudStyle& Style, const FLinearColor& BorderColor);

	/** As Initialize, with FillColor instead of the style's panel fill. */
	void InitializeWithFill(UObject* Outer, const UStartechHudStyle& Style, const FLinearColor& FillColor,
		const FLinearColor& BorderColor);

	/** Changes the fill colour of an initialized panel, for example a button's hover state. */
	void SetFillColor(const FLinearColor& FillColor);

	/** Changes the outline colour of an initialized panel, for example a toggle button's selected state. */
	void SetBorderColor(const FLinearColor& BorderColor);

	/** Draws the panel with its top-left corner at Position. */
	void Draw(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, const UStartechHudStyle& Style) const;

	/** Draws the panel at Opacity (0-1), for example a disabled button at 40%. */
	void DrawWithOpacity(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Position,
		const FVector2f& Size, const UStartechHudStyle& Style, float Opacity) const;

private:
	/** Fill colour, for the fallback drawing. */
	FLinearColor Fill = FLinearColor::Transparent;

	/** The panel material instance, or null when the style has no panel material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	/** Brush drawing Material. */
	FSlateBrush Brush;

	/** Outline colour, for the fallback drawing. */
	FLinearColor Border = FLinearColor::Transparent;

	/** The Size last written to the material, so it is only set when it changes. */
	mutable FVector2f LastSize = FVector2f::ZeroVector;
};
