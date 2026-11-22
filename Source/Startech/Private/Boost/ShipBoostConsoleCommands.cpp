#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Boost/ShipBoostComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "StartechShipPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogStartechBoostConsole, Log, All);

/**
 * Boost debug command (non-shipping):
 *   Startech.Test.Boost                 logs "[Boost]" with the energy, threshold, lockout, multiplier, speed, normal
 *                                       top speed, both cameras' field of view and the chase camera's position lag
 *   Startech.Test.Boost press|release   the boost key down or up, through the same rules as Left Shift (refused in the
 *                                       hub, the pause menu, a warp or when dead)
 *   Startech.Test.Boost throttle <V>    holds the throttle at V (-1..1), for runs without key events (-nullrhi)
 */
namespace StartechBoostConsole
{
	static void Boost(const TArray<FString>& Args, UWorld* const World)
	{
		AStartechShipPawn* const Ship = World ? Cast<AStartechShipPawn>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
		UShipBoostComponent* const BoostComponent = Ship ? Ship->GetBoost() : nullptr;
		if (!BoostComponent)
		{
			UE_LOG(LogStartechBoostConsole, Warning, TEXT("[Boost] No player ship with a boost."));
			return;
		}

		const FString Action = Args.Num() > 0 ? Args[0].ToLower() : FString();
		if (Action == TEXT("press"))
		{
			if (Ship->IsBoostAllowed())
			{
				BoostComponent->SetBoostHeld(true);
			}
			else
			{
				UE_LOG(LogStartechBoostConsole, Log, TEXT("[Boost] press ignored: not allowed now (hub, pause, warp or dead)."));
			}
		}
		else if (Action == TEXT("release"))
		{
			BoostComponent->SetBoostHeld(false);
		}
		else if (Action == TEXT("throttle") && Args.Num() > 1)
		{
			Ship->SetThrottleInput(FCString::Atof(*Args[1]));
		}

		float ChaseFov = 0.f;
		float CockpitFov = 0.f;
		TInlineComponentArray<UCameraComponent*> Cameras(Ship);
		for (const UCameraComponent* const Camera : Cameras)
		{
			(Camera->GetFName() == TEXT("CockpitCamera") ? CockpitFov : ChaseFov) = Camera->FieldOfView;
		}
		const USpringArmComponent* const Arm = Ship->FindComponentByClass<USpringArmComponent>();

		UE_LOG(LogStartechBoostConsole, Log,
			TEXT("[Boost] %s energy %.1f / %.1f (restart %.1f) boosting %d locked %d allowed %d multiplier %.3f speed %.0f (normal top %.0f) chase FOV %.2f cockpit FOV %.2f lag %d @ %.1f"),
			*Action, BoostComponent->GetEnergy(), BoostComponent->GetMaxEnergy(), BoostComponent->GetRestartThreshold(),
			BoostComponent->IsBoosting(), BoostComponent->IsLockedOut(), Ship->IsBoostAllowed(),
			BoostComponent->GetSpeedMultiplier(), Ship->GetSpeed(), Ship->GetNormalMaxForwardSpeed(), ChaseFov, CockpitFov,
			Arm ? Arm->bEnableCameraLag : 0, Arm ? Arm->CameraLagSpeed : 0.f);
	}

	static FAutoConsoleCommandWithWorldAndArgs BoostCommand(TEXT("Startech.Test.Boost"),
		TEXT("Startech.Test.Boost [press|release|throttle <V>]: logs the boost state; press/release act like Left Shift."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Boost));
}

#endif
