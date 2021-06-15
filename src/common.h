#ifndef CLOX_COMMON_H
#define CLOX_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef NDEBUG
#define DEBUG_TRACE_EXEC
#define DEBUG_PRINT_CODE

// This flags allows the GC to run as often as possible
//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC

#endif

#endif
