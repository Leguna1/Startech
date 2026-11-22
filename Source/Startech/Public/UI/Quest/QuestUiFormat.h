#pragma once

#include "CoreMinimal.h"

class UQuestDefinition;
class UStartechHudStyle;
struct FGameplayTag;
struct FQuestReward;
struct FQuestRuntimeState;

/**
 * Quest text and colours shared by the station hub and the HUD quest tracker, so a quest reads the same everywhere:
 * category names and tones, timers, objective lines, summaries, rewards and the turn-in handover.
 */
class STARTECH_API FQuestUiFormat
{
public:
	/** The category's short name, the last segment of its tag: "Story", "Timed", "Bounty". */
	static FText CategoryName(const UQuestDefinition& Definition);

	/** The category's tone: Story in Player, Timed in Hull, Bounty in Credits colour, anything else in Text colour. */
	static FLinearColor CategoryColor(const UQuestDefinition& Definition, const UStartechHudStyle& Style);

	/** Sort rank for the tracker: timed 0, story 1, bounty 2, anything else 3. */
	static int32 CategoryRank(const UQuestDefinition& Definition);

	/** A running quest timer's colour: Text, Hull (amber) below the warn time, Danger below the critical time. */
	static FLinearColor TimerColor(float Seconds, const UStartechHudStyle& Style);

	/** True while the quest's clock runs: Active with a time limit. */
	static bool IsTimerRunning(const UQuestDefinition& Definition, const FQuestRuntimeState& State);

	/** True for a repeatable quest waiting out its cooldown. */
	static bool IsCoolingDown(const FQuestRuntimeState& State);

	/** The objective's count, 0 when the state has none. */
	static int32 GetProgress(const FQuestRuntimeState& State, int32 ObjectiveIndex);

	/** The objective's required count, 0 when it is missing. */
	static int32 GetRequired(const UQuestDefinition& Definition, int32 ObjectiveIndex);

	/** True when the objective's count has reached its required count. */
	static bool IsObjectiveDone(const UQuestDefinition& Definition, const FQuestRuntimeState& State, int32 ObjectiveIndex);

	/** How many objectives are done. */
	static int32 CountDoneObjectives(const UQuestDefinition& Definition, const FQuestRuntimeState& State);

	/** The objective the player works on now: the first incomplete one, INDEX_NONE when every one is done. */
	static int32 FindCurrentObjective(const UQuestDefinition& Definition, const FQuestRuntimeState& State);

	/** The objective's description with {Current} and {Required} filled in. */
	static FText ObjectiveText(const UQuestDefinition& Definition, const FQuestRuntimeState& State, int32 ObjectiveIndex);

	/**
	 * One line under a quest's title: "Story · 1/2 objectives" for several objectives, "Timed · <objective line>" for
	 * one. Not yet accepted: the category and objective count only.
	 */
	static FText Summary(const UQuestDefinition& Definition, const FQuestRuntimeState& State);

	/** "Time limit 4:00", or "Time left 2:41" while the clock runs. Empty without a time limit. */
	static FText TimeLimitText(const UQuestDefinition& Definition, const FQuestRuntimeState& State);

	/** True when the reward is credits (Reward.Credits). */
	static bool IsCreditsReward(const FQuestReward& Reward);

	/** "1,500 credits", or an item as "200 × LL1". */
	static FText RewardText(const FQuestReward& Reward);

	/** The total credits among the quest's rewards. */
	static int32 CreditsReward(const UQuestDefinition& Definition);

	/** An item tag's display name: the quest good's DisplayName, else the tag's last segment. */
	static FText ItemName(const FGameplayTag& ItemTag);

	/** "20 Ferrite will be handed over" for each collect objective that consumes items, one per line. Empty if none. */
	static FText HandoverText(const UQuestDefinition& Definition);
};
