#pragma once

#include "CoreMinimal.h"

/** Every event the HUD listens to is logged here once per broadcast, in non-shipping builds only. */
STARTECH_API DECLARE_LOG_CATEGORY_EXTERN(LogStartechHUD, Log, All);

/**
 * Logs one HUD event at Log verbosity. Compiled out of shipping builds.
 * Use for events that fire on discrete changes.
 */
#if !UE_BUILD_SHIPPING
#define STARTECH_HUD_LOG(Format, ...) UE_LOG(LogStartechHUD, Log, Format, ##__VA_ARGS__)
#else
#define STARTECH_HUD_LOG(Format, ...)
#endif

/**
 * Logs one HUD event at Verbose verbosity. Compiled out of shipping builds.
 * Use for events that can fire many times a second (shield regeneration, docked repair, marker churn).
 * Show them with the console command: Log LogStartechHUD Verbose
 */
#if !UE_BUILD_SHIPPING
#define STARTECH_HUD_LOG_VERBOSE(Format, ...) UE_LOG(LogStartechHUD, Verbose, Format, ##__VA_ARGS__)
#else
#define STARTECH_HUD_LOG_VERBOSE(Format, ...)
#endif
