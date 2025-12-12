#ifndef ZCOMMON_H
#define ZCOMMON_H

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Return Codes.
#define Z_OK     0
#define Z_ERR   -1
#define Z_FOUND  1

// Memory Macros.
// If the user hasn't defined their own allocator, use the standard one.
#ifndef Z_MALLOC
    #include <stdlib.h>
    #define Z_MALLOC(sz)       malloc(sz)
    #define Z_CALLOC(n, sz)    calloc(n, sz)
    #define Z_REALLOC(p, sz)   realloc(p, sz)
    #define Z_FREE(p)          free(p)
#endif

// Compiler Extensions (Optional).
// We check for GCC/Clang features to enable RAII-style cleanup.
// Define Z_NO_EXTENSIONS to disable this manually.
#if !defined(Z_NO_EXTENSIONS) && (defined(__GNUC__) || defined(__clang__))
        
#   define Z_HAS_CLEANUP 1
    
    // RAII Cleanup (destructors).
#   define Z_CLEANUP(func) __attribute__((cleanup(func)))
    
    // Warn if the return value (for example, an Error Result) is ignored.
    #define Z_NODISCARD     __attribute__((warn_unused_result))
    
    // Branch prediction hints.
#   define Z_LIKELY(x)     __builtin_expect(!!(x), 1)
#   define Z_UNLIKELY(x)   __builtin_expect(!!(x), 0)

#else
        
#   define Z_HAS_CLEANUP 0
#   define Z_CLEANUP(func) 
#   define Z_NODISCARD
#   define Z_LIKELY(x)     (x)
#   define Z_UNLIKELY(x)   (x)

#endif

// Metaprogramming Markers.
// These macros are used by the Z-Scanner tool to find type definitions.
// For the C compiler, they are no-ops (they compile to nothing).
#define DEFINE_VEC_TYPE(T, Name)
#define DEFINE_LIST_TYPE(T, Name)
#define DEFINE_MAP_TYPE(Key, Val, Name)
#define DEFINE_STABLE_MAP_TYPE(Key, Val, Name)

// Token concatenation macros (useful for unique variable names in defer)
#define Z_CONCAT_(a, b) a ## b
#define Z_CONCAT(a, b) Z_CONCAT_(a, b)
#define Z_UNIQUE(prefix) Z_CONCAT(prefix, __LINE__)

// Growth Strategy.
// Determines how containers expand when full.
// Default is 2.0x (Geometric Growth).
//
// Optimization Note:
// 2.0x minimizes realloc calls but can waste memory.
// 1.5x is often better for memory fragmentation and reuse.
#ifndef Z_GROWTH_FACTOR
    // Default: Double capacity (2.0x)
    #define Z_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) * 2)
    
    // Alternative: 1.5x Growth (Uncomment to use)
    // #define Z_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) + (cap) / 2)
#endif

#endif
