#include "UI/HUD/HudLayoutEditLibrary.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/InvalidationBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/HUD/HudActionBarWidget.h"
#include "UI/HUD/HudSpeedReadoutWidget.h"
#include "WidgetBlueprint.h"

namespace HudLayoutEdit
{
	/** The widget that places Widget in the root canvas: Widget itself or the ancestor directly under the canvas. */
	static UWidget* LayoutOf(UWidget* Widget)
	{
		while (Widget && Widget->GetParent() && !Widget->GetParent()->IsA<UCanvasPanel>())
		{
			Widget = Widget->GetParent();
		}
		return Widget;
	}

	/** Sets a canvas slot's layout. */
	static void Place(UCanvasPanelSlot& Slot, const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size)
	{
		Slot.SetAnchors(FAnchors(0.5f, 1.f));
		Slot.SetAlignment(Alignment);
		Slot.SetPosition(Position);
		Slot.SetSize(Size);
		Slot.SetAutoSize(false);
	}
}

TArray<FString> UHudLayoutEditLibrary::DescribeWidgetTree(UObject* const WidgetBlueprint)
{
	TArray<FString> Lines;
	const UWidgetBlueprint* const Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprint);
	if (!Blueprint || !Blueprint->WidgetTree)
	{
		return Lines;
	}

	Blueprint->WidgetTree->ForEachWidget([&Lines](UWidget* const Widget)
	{
		int32 Depth = 0;
		for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		{
			++Depth;
		}
		FString Line = FString::Printf(TEXT("%s%s (%s)"), *FString::ChrN(Depth * 2, TEXT(' ')), *Widget->GetName(),
			*Widget->GetClass()->GetName());
		if (const UCanvasPanelSlot* const Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			const FAnchors Anchors = Slot->GetAnchors();
			Line += FString::Printf(TEXT(" anchors %s-%s align %s pos %s size %s%s"), *Anchors.Minimum.ToString(),
				*Anchors.Maximum.ToString(), *Slot->GetAlignment().ToString(), *Slot->GetPosition().ToString(),
				*Slot->GetSize().ToString(), Slot->GetAutoSize() ? TEXT(" autosize") : TEXT(""));
		}
		Lines.Add(Line);
	});
	return Lines;
}

bool UHudLayoutEditLibrary::ApplyActionBarLayout(UObject* const WidgetBlueprint, TArray<FString>& OutReport)
{
	using namespace HudLayoutEdit;

	UWidgetBlueprint* const Blueprint = Cast<UWidgetBlueprint>(WidgetBlueprint);
	UWidgetTree* const Tree = Blueprint ? Blueprint->WidgetTree.Get() : nullptr;
	UCanvasPanel* const Canvas = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	if (!Canvas)
	{
		OutReport.Add(TEXT("Not a Widget Blueprint with a Canvas Panel root; nothing changed."));
		return false;
	}
	Blueprint->Modify();
	Tree->Modify();

	// The weapon strip and the old slot widgets were removed by the first run of this (2026-11-21); their classes
	// no longer exist, so only the bar and the readout are placed now.

	// 1. The action bar in its Invalidation Box.
	UHudActionBarWidget* Bar = nullptr;
	Tree->ForEachWidget([&Bar](UWidget* const Widget)
	{
		if (!Bar)
		{
			Bar = Cast<UHudActionBarWidget>(Widget);
		}
	});
	if (!Bar)
	{
		// Removed widgets keep their objects (and names, which ignore case) in the tree: never reuse a name.
		const FName BoxName = MakeUniqueObjectName(Tree, UInvalidationBox::StaticClass(), TEXT("IB_ActionBar"));
		const FName BarName = MakeUniqueObjectName(Tree, UHudActionBarWidget::StaticClass(), TEXT("HudActionBar"));
		UInvalidationBox* const Box = Tree->ConstructWidget<UInvalidationBox>(UInvalidationBox::StaticClass(), BoxName);
		Bar = Tree->ConstructWidget<UHudActionBarWidget>(UHudActionBarWidget::StaticClass(), BarName);
		Box->SetContent(Bar);
		Canvas->AddChildToCanvas(Box);
		Blueprint->OnVariableAdded(Box->GetFName());
		Blueprint->OnVariableAdded(Bar->GetFName());
		OutReport.Add(FString::Printf(TEXT("Added %s (InvalidationBox) holding %s (HudActionBarWidget)"), *BoxName.ToString(),
			*BarName.ToString()));
	}
	if (UCanvasPanelSlot* const BarSlot = Cast<UCanvasPanelSlot>(LayoutOf(Bar)->Slot))
	{
		Place(*BarSlot, FVector2D(0.5, 1.0), FVector2D(0.0, -28.0), FVector2D(376.0, 96.0));
		OutReport.Add(FString::Printf(TEXT("Placed %s: bottom centre, alignment (0.5, 1), position (0, -28), size 376 x 96"),
			*LayoutOf(Bar)->GetName()));
	}

	// 2. The speed readout right of the bar.
	Tree->ForEachWidget([&OutReport](UWidget* const Widget)
	{
		if (Widget->IsA<UHudSpeedReadoutWidget>())
		{
			if (UCanvasPanelSlot* const ReadoutSlot = Cast<UCanvasPanelSlot>(LayoutOf(Widget)->Slot))
			{
				Place(*ReadoutSlot, FVector2D(0.0, 1.0), FVector2D(204.0, -42.0), FVector2D(120.0, 64.0));
				OutReport.Add(FString::Printf(TEXT("Placed %s: bottom centre, alignment (0, 1), position (204, -42), size 120 x 64"),
					*LayoutOf(Widget)->GetName()));
			}
		}
	});

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	OutReport.Add(FString::Printf(TEXT("Compiled %s: %s"), *Blueprint->GetName(),
		Blueprint->Status == BS_Error ? TEXT("ERRORS") : TEXT("ok")));
	return Blueprint->Status != BS_Error;
}
#endif
