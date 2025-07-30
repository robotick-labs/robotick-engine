#pragma once

#include "robotick/framework/common/FixedString.h"

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

inline const char* robotick_filename(const char* path)
{
	const char* slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

#define ROBOTICK_INTERNAL_LOG(level, fmt, ...) fprintf(stderr, "[%s] %s:%d: " fmt "\n", level, robotick_filename(__FILE__), __LINE__, ##__VA_ARGS__);

// =====================================================================
// ✅ TEST ERROR HANDLER
// =====================================================================

namespace robotick
{
#if defined(ROBOTICK_TEST_MODE)
	class TestError : public std::exception
	{
	  public:
		explicit TestError(const char* message)
			: msg(message)
		{
		}
		const char* what() const noexcept override { return msg.c_str(); }

	  private:
		FixedString256 msg;
	};

	inline void report_error(const char* message)
	{
		fprintf(stderr, "\033[1;31m[ERROR] %s\033[0m\n", message);
		throw TestError(message);
	}
#else
	inline void report_error(const char* message)
	{
		ROBOTICK_BREAKPOINT();
		fprintf(stderr, "\033[1;31m[ERROR] %s\033[0m\n", message);
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
			ROBOTICK_FATAL_EXIT("Assertion failed: %s", #cond);                                                                                      \
		}                                                                                                                                            \
	} while (0)

#define ROBOTICK_ASSERT_MSG(cond, fmt, ...)                                                                                                          \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!(cond))                                                                                                                                 \
		{                                                                                                                                            \
			ROBOTICK_FATAL_EXIT("Assertion failed: %s - " fmt, #cond, ##__VA_ARGS__);                                                                \
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

#define ROBOTICK_FATAL_EXIT(...)                                                                                                                     \
	do                                                                                                                                               \
	{                                                                                                                                                \
		char __robotick_error_buf[1024];                                                                                                             \
		snprintf(__robotick_error_buf, sizeof(__robotick_error_buf), __VA_ARGS__);                                                                   \
		robotick::report_error(__robotick_error_buf);                                                                                                \
	} while (0) /* @noreturn */

#define ROBOTICK_WARNING(...)                                                                                                                        \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_BREAKPOINT();                                                                                                                       \
		ROBOTICK_INTERNAL_LOG("WARN", __VA_ARGS__);                                                                                                  \
	} while (0)

#define ROBOTICK_WARNING_ONCE(...)                                                                                                                   \
	do                                                                                                                                               \
	{                                                                                                                                                \
		static bool robotick_warning_logged = false;                                                                                                 \
		if (!robotick_warning_logged)                                                                                                                \
		{                                                                                                                                            \
			ROBOTICK_BREAKPOINT();                                                                                                                   \
			ROBOTICK_INTERNAL_LOG("WARN", __VA_ARGS__);                                                                                              \
			robotick_warning_logged = true;                                                                                                          \
		}                                                                                                                                            \
	} while (0)

#define ROBOTICK_INFO(...)                                                                                                                           \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_INTERNAL_LOG("INFO", __VA_ARGS__);                                                                                                  \
	} while (0);

// =====================================================================
// ✅ TEST MACROS
// =====================================================================

#define ROBOTICK_REQUIRE_ERROR_MSG(expr, substr_literal)                                                                                             \
	do                                                                                                                                               \
	{                                                                                                                                                \
		try                                                                                                                                          \
		{                                                                                                                                            \
			expr;                                                                                                                                    \
			FAIL("Expected TestError to be thrown");                                                                                                 \
		}                                                                                                                                            \
		catch (const robotick::TestError& e)                                                                                                         \
		{                                                                                                                                            \
			REQUIRE_THAT(std::string(e.what()), Catch::Matchers::ContainsSubstring(substr_literal));                                                 \
		}                                                                                                                                            \
	} while (0)

#define ROBOTICK_REQUIRE_ERROR(expr)                                                                                                                 \
	do                                                                                                                                               \
	{                                                                                                                                                \
		try                                                                                                                                          \
		{                                                                                                                                            \
			expr;                                                                                                                                    \
			FAIL("Expected TestError to be thrown");                                                                                                 \
		}                                                                                                                                            \
		catch (const robotick::TestError&)                                                                                                           \
		{                                                                                                                                            \
			/*success*/                                                                                                                              \
		}                                                                                                                                            \
	} while (0)
