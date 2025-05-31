#pragma once

#include "robotick/config/FeatureFlags.h"
#include <stdio.h>
#include <stdlib.h>

// =====================================================================
// 🚨 BREAKPOINT + LOGGING
// =====================================================================

#if defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
#define ROBOTICK_BREAKPOINT() __builtin_debugtrap()
#elif __has_builtin(__debugbreak)
#define ROBOTICK_BREAKPOINT() __debugbreak()
#else
#define ROBOTICK_BREAKPOINT() ((void)0)
#endif
#elif defined(_MSC_VER)
#define ROBOTICK_BREAKPOINT() __debugbreak()
#else
#define ROBOTICK_BREAKPOINT() __builtin_trap()
#endif

#define ROBOTICK_INTERNAL_LOG(level, fmt, ...) fprintf(stderr, "[%s] %s:%d: " fmt "\n", level, __FILE__, __LINE__, ##__VA_ARGS__)

// =====================================================================
// ✅ ASSERTIONS (hard / soft)
// =====================================================================

#define ROBOTICK_ASSERT(cond)                                                                                                                        \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!(cond))                                                                                                                                 \
		{                                                                                                                                            \
			ROBOTICK_BREAKPOINT();                                                                                                                   \
			ROBOTICK_INTERNAL_LOG("ASSERT", "Assertion failed: %s", #cond);                                                                          \
			exit(1);                                                                                                                                 \
		}                                                                                                                                            \
	} while (0)

#define ROBOTICK_ASSERT_MSG(cond, fmt, ...)                                                                                                          \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!(cond))                                                                                                                                 \
		{                                                                                                                                            \
			ROBOTICK_BREAKPOINT();                                                                                                                   \
			ROBOTICK_INTERNAL_LOG("ASSERT", "Assertion failed: %s - " fmt, #cond, ##__VA_ARGS__);                                                    \
			exit(1);                                                                                                                                 \
		}                                                                                                                                            \
	} while (0)

#define ROBOTICK_ASSERT_SOFT(cond)                                                                                                                   \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!(cond))                                                                                                                                 \
		{                                                                                                                                            \
			ROBOTICK_BREAKPOINT();                                                                                                                   \
			ROBOTICK_INTERNAL_LOG("WARN", "Soft assert failed: %s", #cond);                                                                          \
		}                                                                                                                                            \
	} while (0)

#define ROBOTICK_ASSERT_SOFT_MSG(cond, fmt, ...)                                                                                                     \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!(cond))                                                                                                                                 \
		{                                                                                                                                            \
			ROBOTICK_BREAKPOINT();                                                                                                                   \
			ROBOTICK_INTERNAL_LOG("WARN", "Soft assert failed: %s - " fmt, #cond, ##__VA_ARGS__);                                                    \
		}                                                                                                                                            \
	} while (0)

// =====================================================================
// ✅ ERRORS & WARNINGS (printf or plain string)
// =====================================================================

#define ROBOTICK_ERROR(...)                                                                                                                          \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_BREAKPOINT();                                                                                                                       \
		ROBOTICK_INTERNAL_LOG("ERROR", __VA_ARGS__);                                                                                                 \
		exit(1);                                                                                                                                     \
	} while (0)

#define ROBOTICK_WARNING(...)                                                                                                                        \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_BREAKPOINT();                                                                                                                       \
		ROBOTICK_INTERNAL_LOG("WARN", __VA_ARGS__);                                                                                                  \
	} while (0)
