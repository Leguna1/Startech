#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HudMaterialCheckLibrary.generated.h"

class UMaterialInterface;

/**
 * Editor-only check used by the HUD material build scripts (Tools/HUD). MaterialEditingLibrary.recompile_material
 * only reports errors from translating the graph; shader compiler errors (such as a Slate pixel shader failing) only
 * appear once the shaders have actually compiled. This waits for that and returns the result.
 */
UCLASS()
class STARTECH_API UHudMaterialCheckLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Finishes compiling Material's shaders for the editor's shader platform, and returns every compile error, plus
	 * one if the material ends up with no valid shader map. Empty means it compiled. OutPlatform names the platform
	 * checked.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Editor")
	static TArray<FString> GetMaterialCompileErrors(UMaterialInterface* Material, FString& OutPlatform);
#endif
};
