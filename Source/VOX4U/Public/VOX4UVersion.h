// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

/**
 * Engine version checks of the plugin, in one place.
 *
 * The code is written for the newest Unreal Engine; only where an API does not exist in an older version
 * the old way is used, behind a check:
 *
 *     #if VOX4U_UE_AT_LEAST(5, 1)
 *         ... current API ...
 *     #else
 *         ... UE 5.0 ...
 *     #endif
 */
#define VOX4U_UE_AT_LEAST(Major, Minor) \
	(ENGINE_MAJOR_VERSION > (Major) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))
