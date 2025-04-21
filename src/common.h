#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define DEBUG_PRINT_TOKENS
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_STRESS_GC

// #define DEBUG_LOG_GC

#ifdef DEBUG_LOG_GC
#define DEBUG_LOG_GC_MARK
#define DEBUG_LOG_GC_BLACKEN
#define DEBUG_LOG_GC_SWEEP
#define DEBUG_LOG_GC_FREE
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif