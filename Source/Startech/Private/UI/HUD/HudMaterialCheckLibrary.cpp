#include "UI/HUD/HudMaterialCheckLibrary.h"

#if WITH_EDITOR
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "RHIShaderPlatform.h"
#include "DataDrivenShaderPlatformInfo.h"

TArray<FString> UHudMaterialCheckLibrary::GetMaterialCompileErrors(UMaterialInterface* const Material, FString& OutPlatform)
{
	OutPlatform = FDataDrivenShaderPlatformInfo::GetName(GMaxRHIShaderPlatform).ToString();
	if (!Material)
	{
		return { TEXT("No material given.") };
	}

	FMaterialResource* const Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
	if (!Resource)
	{
		return { FString::Printf(TEXT("%s has no material resource for %s."), *Material->GetName(), *OutPlatform) };
	}

	if (!Resource->IsGameThreadShaderMapComplete())
	{
		Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
	}
	Resource->FinishCompilation();

	TArray<FString> Errors = Resource->GetCompileErrors();
	if (Errors.Num() == 0 && !Resource->HasValidGameThreadShaderMap())
	{
		Errors.Add(FString::Printf(TEXT("%s has no valid shader map for %s after compiling."), *Material->GetName(),
			*OutPlatform));
	}
	return Errors;
}
#endif
