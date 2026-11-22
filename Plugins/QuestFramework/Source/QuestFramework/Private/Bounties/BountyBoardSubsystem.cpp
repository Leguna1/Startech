#include "Bounties/BountyBoardSubsystem.h"

#include "Bounties/BountyFactory.h"
#include "Bounties/BountyTemplate.h"
#include "Definitions/QuestDefinition.h"
#include "Engine/GameInstance.h"
#include "Math/RandomStream.h"
#include "QuestFrameworkLog.h"
#include "QuestFrameworkSettings.h"
#include "Runtime/QuestSubsystem.h"

namespace BountyBoard
{
	/** Salt mixed into the seed when picking a template, so the pick is independent of the bounty's own draws. */
	constexpr uint32 TemplatePickSalt = 0xB0A2D5EE;
}

void UBountyBoardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UQuestSubsystem>();
	Super::Initialize(Collection);

	// Positive and well away from overflow; a save replaces it with SetNextSeed.
	NextSeed = FMath::RandRange(1, 1 << 24);
}

void UBountyBoardSubsystem::Refresh()
{
	UQuestSubsystem* const Quests = GetQuests();
	if (!Quests)
	{
		return;
	}

	// Unaccepted offers go; accepted (or completed) bounties stay registered.
	for (const UQuestDefinition* const Offer : GetOffers())
	{
		Quests->UnregisterDefinition(Offer->QuestId);
	}

	const UQuestFrameworkSettings& Settings = UQuestFrameworkSettings::Get();
	const TArray<const UBountyTemplate*> Templates = Settings.LoadBountyTemplates();
	if (Templates.IsEmpty())
	{
		UE_LOG(LogQuestFramework, Warning, TEXT("Bounty board: no templates in Project Settings > Plugins > Quest Framework."));
	}

	for (int32 Offer = 0; Offer < Settings.BountyBoardSize && !Templates.IsEmpty(); ++Offer)
	{
		const int32 Seed = NextSeed++;
		const UBountyTemplate* const Template = PickTemplate(Templates, Seed);
		if (UQuestDefinition* const Definition = Template ? FBountyFactory::Generate(*Template, Seed, Quests) : nullptr)
		{
			Quests->RegisterDefinition(Definition);
		}
	}

	UE_LOG(LogQuestFramework, Log, TEXT("Bounty board refreshed: %d offer(s)."), GetOffers().Num());
	OnBoardRefreshed.Broadcast();
}

TArray<UQuestDefinition*> UBountyBoardSubsystem::GetOffers() const
{
	TArray<UQuestDefinition*> Offers;
	const UQuestSubsystem* const Quests = GetQuests();
	if (!Quests)
	{
		return Offers;
	}

	for (UQuestDefinition* const Definition : Quests->GetAllDefinitions())
	{
		const FQuestRuntimeState* const State = Quests->FindState(Definition->QuestId);
		if (Definition->IsGenerated() && State && State->Status == EQuestStatus::Available)
		{
			Offers.Add(Definition);
		}
	}
	return Offers;
}

const UBountyTemplate* UBountyBoardSubsystem::PickTemplate(const TArray<const UBountyTemplate*>& Templates, const int32 Seed)
{
	if (Templates.IsEmpty())
	{
		return nullptr;
	}
	const FRandomStream Stream(static_cast<int32>(HashCombine(static_cast<uint32>(Seed), BountyBoard::TemplatePickSalt)));
	return Templates[Stream.RandRange(0, Templates.Num() - 1)];
}

UQuestSubsystem* UBountyBoardSubsystem::GetQuests() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UQuestSubsystem>() : nullptr;
}
