#pragma once

#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include <string>

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
// ✅ TEST ERROR HANDLER
// =====================================================================

namespace robotick
{
#if defined(ROBOTICK_TEST_MODE)
	class TestError : public std::exception
	{
	  public:
		explicit TestError(std::string message) : msg(std::move(message)) {}
		const char* what() const noexcept override { return msg.c_str(); }

	  private:
		std::string msg;
	};

	inline void report_error(const std::string& message)
	{
		fprintf(stderr, "\033[1;31m[ERROR] %s\033[0m\n", message.c_str());
		throw TestError(message);
	}
#else
	inline void report_error(const std::string& message)
	{
		ROBOTICK_BREAKPOINT();
		fprintf(stderr, "\033[1;31m[ERROR] %s\033[0m\n", message.c_str());
		exit(1);
	}
#endif
} // namespace robotick

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
		char __robotick_error_buf[1024];                                                                                                             \
		snprintf(__robotick_error_buf, sizeof(__robotick_error_buf), __VA_ARGS__);                                                                   \
		robotick::report_error(__robotick_error_buf);                                                                                                \
	} while (0)

#define ROBOTICK_WARNING(...)                                                                                                                        \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_BREAKPOINT();                                                                                                                       \
		ROBOTICK_INTERNAL_LOG("WARN", __VA_ARGS__);                                                                                                  \
	} while (0)

#define ROBOTICK_INFO(...)                                                                                                                           \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_BREAKPOINT();                                                                                                                       \
	} while (0)

// =====================================================================
// ✅ TEST MACROS
// =====================================================================

#if defined(ROBOTICK_TEST_MODE)
#include <catch2/catch_all.hpp>
#define ROBOTICK_REQUIRE_ERROR(expr, matcher) REQUIRE_THROWS_AS(expr, robotick::TestError)
#endif
