#include "QuestFrameworkSettings.h"

#include "Bounties/BountyTemplate.h"
#include "QuestTags.h"

UQuestFrameworkSettings::UQuestFrameworkSettings()
{
	CategoryName = TEXT("Plugins");

	FDirectoryPath& Quests = QuestDefinitionPaths.AddDefaulted_GetRef();
	Quests.Path = TEXT("/Game/Quests");

	const auto AddLimit = [this](const FGameplayTag& Category, const int32 Max)
	{
		FQuestCategoryLimit& Limit = CategoryLimits.AddDefaulted_GetRef();
		Limit.Category = Category;
		Limit.Max = Max;
	};
	AddLimit(QuestTags::Category_Story, 1);
	AddLimit(QuestTags::Category_Bounty, 3);
	AddLimit(QuestTags::Category_Timed, 1);
}

const UQuestFrameworkSettings& UQuestFrameworkSettings::Get()
{
	return *GetDefault<UQuestFrameworkSettings>();
}

TArray<const UBountyTemplate*> UQuestFrameworkSettings::LoadBountyTemplates() const
{
	TArray<const UBountyTemplate*> Loaded;
	for (const TSoftObjectPtr<UBountyTemplate>& Entry : BountyTemplates)
	{
		if (const UBountyTemplate* const Template = Entry.LoadSynchronous())
		{
			Loaded.Add(Template);
		}
	}
	return Loaded;
}

const UBountyTemplate* UQuestFrameworkSettings::FindBountyTemplate(const FName TemplateId) const
{
	const TArray<const UBountyTemplate*> Templates = LoadBountyTemplates();
	const UBountyTemplate* const* const Found = Templates.FindByPredicate([TemplateId](const UBountyTemplate* Template)
	{
		return Template->TemplateId == TemplateId;
	});
	return Found ? *Found : nullptr;
}

int32 UQuestFrameworkSettings::GetCategoryLimit(const FGameplayTag& Category) const
{
	const FQuestCategoryLimit* const Limit = FindCategoryLimit(Category);
	return Limit ? Limit->Max : INDEX_NONE;
}

const FQuestCategoryLimit* UQuestFrameworkSettings::FindCategoryLimit(const FGameplayTag& Category) const
{
	return CategoryLimits.FindByPredicate([&Category](const FQuestCategoryLimit& Each)
	{
		return Each.Category == Category;
	});
}
