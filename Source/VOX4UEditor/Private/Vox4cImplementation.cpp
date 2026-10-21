// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

// The one translation unit that compiles the vox4c implementation.
// Allocations go through FMemory so they show up in Unreal's memory tracking.

#include "CoreMinimal.h"
#include "HAL/UnrealMemory.h"

#define V4C_MALLOC(Size)        FMemory::Malloc(Size)
#define V4C_REALLOC(Ptr, Size)  FMemory::Realloc(Ptr, Size)
#define V4C_FREE(Ptr)           FMemory::Free(Ptr)
// Files are read by the editor and handed over as memory, v4c_load_file is not needed.
#define V4C_NO_STDIO

THIRD_PARTY_INCLUDES_START
#define VOX4C_IMPLEMENTATION
#include "vox4c.h"
#undef VOX4C_IMPLEMENTATION
THIRD_PARTY_INCLUDES_END

#undef V4C_MALLOC
#undef V4C_REALLOC
#undef V4C_FREE
#undef V4C_NO_STDIO
