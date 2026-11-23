#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MusicPlaylist.generated.h"

class USoundBase;

/**
 * DA_Music_*: the takes one sector (or the main menu) plays. UStartechMusicSubsystem plays a random take, then another
 * random take (never the same one twice in a row) when it ends, so the tracks themselves need not loop. Two sectors
 * naming the same playlist share it: moving between them never restarts the music. Made by Tools/Audio/setup_audio.py.
 */
UCLASS(BlueprintType)
class STARTECH_API UMusicPlaylist : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The takes, loaded one at a time when picked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music")
	TArray<TSoftObjectPtr<USoundBase>> Tracks;
};
