#include "StartechHUD.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

void AStartechHUD::BeginPlay()
{
	Super::BeginPlay();

	if (GameHudClass && PlayerOwner)
	{
		GameHud = CreateWidget<UUserWidget>(PlayerOwner, GameHudClass);
		if (GameHud)
		{
			GameHud->AddToViewport();
		}
	}
}
