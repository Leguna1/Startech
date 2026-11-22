#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StartechHUD.generated.h"

class UUserWidget;

// Creates the widget HUD for the owning player. All drawing lives in the widgets (WB_GameHUD, whose C++ parent is
// UStartechHudWidget); the canvas draws nothing.
UCLASS()
class STARTECH_API AStartechHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// The HUD's root widget (WB_GameHUD), added to the viewport for the owning player.
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UUserWidget> GameHudClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GameHud;
};
