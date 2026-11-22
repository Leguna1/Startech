#pragma once

#include "CoreMinimal.h"

class UBountyTemplate;
class UObject;
class UQuestDefinition;
struct FRandomStream;

/**
 * Builds bounty quests from templates (Factory pattern). Deterministic: the same template and seed always give the
 * same quest, which is how saved bounties are regenerated after loading.
 */
class QUESTFRAMEWORK_API FBountyFactory
{
public:
	/**
	 * Generates a transient quest definition with one Destroy objective and a reward computed from the template:
	 * count x RewardPerUnit x the location's multiplier, given as CreditRewardTag, plus ExtraRewards.
	 * The quest id is Bounty_<TemplateId>_<Seed>. Returns null if the template has no targets.
	 */
	static UQuestDefinition* Generate(const UBountyTemplate& Template, int32 Seed, UObject* Outer);

	/** The id Generate gives the quest for this template and seed. */
	static FName MakeQuestId(FName TemplateId, int32 Seed);

private:
	/** Picks an index by weight with exactly one draw from Stream (uniform when every weight is 0). */
	template <typename OptionType>
	static int32 PickWeighted(const TArray<OptionType>& Options, const FRandomStream& Stream);
};
