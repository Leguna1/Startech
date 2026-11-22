#include "UI/HUD/HudPanelBrush.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "UI/HUD/HudDraw.h"
#include "UI/HUD/StartechHudStyle.h"

namespace HudPanel
{
	const FName FillColor(TEXT("FillColor"));
	const FName BorderColor(TEXT("BorderColor"));
	const FName BorderWidth(TEXT("BorderWidth"));
	const FName CornerSize(TEXT("CornerSize"));
	const FName Size(TEXT("Size"));
}

void FHudPanelBrush::Initialize(UObject* const Outer, const UStartechHudStyle& Style, const FLinearColor& BorderColor)
{
	InitializeWithFill(Outer, Style, Style.PanelFillColor, BorderColor);
}

void FHudPanelBrush::InitializeWithFill(UObject* const Outer, const UStartechHudStyle& Style, const FLinearColor& FillColor,
	const FLinearColor& BorderColor)
{
	Border = BorderColor;
	Fill = FillColor;
	LastSize = FVector2f::ZeroVector;
	Material = Style.PanelMaterial ? UMaterialInstanceDynamic::Create(Style.PanelMaterial, Outer) : nullptr;
	if (!Material)
	{
		return;
	}

	Material->SetVectorParameterValue(HudPanel::FillColor, FillColor);
	Material->SetVectorParameterValue(HudPanel::BorderColor, BorderColor);
	Material->SetScalarParameterValue(HudPanel::BorderWidth, Style.PanelBorderWidth);
	Material->SetScalarParameterValue(HudPanel::CornerSize, Style.PanelCornerSize);

	Brush = FSlateBrush();
	Brush.SetResourceObject(Material);
	Brush.DrawAs = ESlateBrushDrawType::Image;
}

void FHudPanelBrush::SetFillColor(const FLinearColor& FillColor)
{
	if (Fill.Equals(FillColor))
	{
		return;
	}
	Fill = FillColor;
	if (UMaterialInstanceDynamic* const Instance = Material.Get())
	{
		Instance->SetVectorParameterValue(HudPanel::FillColor, FillColor);
	}
}

void FHudPanelBrush::SetBorderColor(const FLinearColor& BorderColor)
{
	if (Border.Equals(BorderColor))
	{
		return;
	}
	Border = BorderColor;
	if (UMaterialInstanceDynamic* const Instance = Material.Get())
	{
		Instance->SetVectorParameterValue(HudPanel::BorderColor, BorderColor);
	}
}

void FHudPanelBrush::Draw(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const FVector2f& Size, const UStartechHudStyle& Style) const
{
	DrawWithOpacity(Out, Layer, Geometry, Position, Size, Style, 1.f);
}

void FHudPanelBrush::DrawWithOpacity(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry,
	const FVector2f& Position, const FVector2f& Size, const UStartechHudStyle& Style, const float Opacity) const
{
	if (UMaterialInstanceDynamic* const Instance = Material.Get())
	{
		if (!Size.Equals(LastSize, 0.5f))
		{
			LastSize = Size;
			Instance->SetVectorParameterValue(HudPanel::Size, FLinearColor(Size.X, Size.Y, 0.f, 0.f));
		}
		FHudDraw::BrushBox(Out, Layer, Geometry, Position, Size, Brush, FHudDraw::WithOpacity(FLinearColor::White, Opacity));
		return;
	}

	FHudDraw::Box(Out, Layer, Geometry, Position, Size, FHudDraw::WithOpacity(Fill, Opacity));
	FHudDraw::ChamferOutline(Out, Layer, Geometry, Position, Size, Style.PanelCornerSize,
		FHudDraw::WithOpacity(Border, Opacity), FMath::Max(Style.PanelBorderWidth, 1.f));
}
