#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "QuestTypes.h"
#include "QuestFrameworkSettings.generated.h"

class UBountyTemplate;

/** Quest Framework settings (Project Settings > Plugins > Quest Framework), saved in DefaultGame.ini. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Quest Framework"))
class QUESTFRAMEWORK_API UQuestFrameworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Places the settings under Plugins and sets the default category limits (Story 1, Bounty 3, Timed 1). */
	UQuestFrameworkSettings();

	/** The settings' class default object. */
	static const UQuestFrameworkSettings& Get();

	/** Folders searched (with subfolders) for UQuestDefinition assets when the game starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Quests", meta = (ContentDir, LongPackageName))
	TArray<FDirectoryPath> QuestDefinitionPaths;

	/** The most quests of a category active at once. A category with no entry has no limit. */
	UPROPERTY(Config, EditAnywhere, Category = "Quests")
	TArray<FQuestCategoryLimit> CategoryLimits;

	/** How many offers the bounty board holds after a refresh. */
	UPROPERTY(Config, EditAnywhere, Category = "Bounties", meta = (ClampMin = "0"))
	int32 BountyBoardSize = 3;

	/** The templates the bounty board draws from, and that saved bounties are regenerated from. */
	UPROPERTY(Config, EditAnywhere, Category = "Bounties")
	TArray<TSoftObjectPtr<UBountyTemplate>> BountyTemplates;

	/** Loads (if needed) and returns every valid template in BountyTemplates. */
	TArray<const UBountyTemplate*> LoadBountyTemplates() const;

	/** The template in BountyTemplates with this TemplateId, loaded, or null. */
	const UBountyTemplate* FindBountyTemplate(FName TemplateId) const;

	/** The limit for Category, or INDEX_NONE when it has none. */
	int32 GetCategoryLimit(const FGameplayTag& Category) const;

	/** The limit entry for Category, or null when it has none. */
	const FQuestCategoryLimit* FindCategoryLimit(const FGameplayTag& Category) const;
};
