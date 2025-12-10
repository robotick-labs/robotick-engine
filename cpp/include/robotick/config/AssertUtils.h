// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/memory/StdApproved.h"
#include "robotick/framework/strings/FixedString.h"
#include "robotick/framework/strings/StringUtils.h"

#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__) || defined(__APPLE__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>

#if defined(__linux__)
inline bool robotick_resolve_module_from_proc_maps(void* addr, char* out_path, size_t out_path_size, uintptr_t& base_addr_out)
{
	if (!addr)
		return false;

	FILE* maps = fopen("/proc/self/maps", "r");
	if (!maps)
		return false;

	char line[512];
	while (fgets(line, sizeof(line), maps))
	{
		unsigned long long start = 0;
		unsigned long long end = 0;
		char perms[5] = {};
		unsigned long long offset = 0;
		unsigned int dev_major = 0;
		unsigned int dev_minor = 0;
		unsigned long long inode = 0;
		int path_index = 0;

		const int parsed =
			sscanf(line, "%llx-%llx %4s %llx %x:%x %llu %n", &start, &end, perms, &offset, &dev_major, &dev_minor, &inode, &path_index);
		if (parsed < 7)
			continue;

		const uintptr_t addr_value = reinterpret_cast<uintptr_t>(addr);
		if (addr_value < start || addr_value >= end)
			continue;

		const char* path = line + path_index;
		while (*path == ' ' || *path == '\t')
			++path;

		size_t len = strcspn(path, "\n");
		if (len == 0)
			break;

		if (out_path_size > 0)
		{
			const size_t copy_len = (len < out_path_size - 1) ? len : (out_path_size - 1);
			memcpy(out_path, path, copy_len);
			out_path[copy_len] = '\0';
		}
		base_addr_out = static_cast<uintptr_t>(start);
		fclose(maps);
		return true;
	}

	fclose(maps);
	return false;
}
#else
inline bool robotick_resolve_module_from_proc_maps(void*, char*, size_t, uintptr_t&)
{
	return false;
}
#endif
#endif

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

#define ROBOTICK_INTERNAL_LOG_RAW(level, file, line, func, fmt, ...)                                                                                 \
	fprintf(stderr, "[%s] %s:%d (%s): " fmt "\n", level, robotick_filename(file), line, func, ##__VA_ARGS__)

#define ROBOTICK_INTERNAL_LOG(level, fmt, ...) ROBOTICK_INTERNAL_LOG_RAW(level, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// =====================================================================
// ✅ TEST ERROR HANDLER
// =====================================================================

namespace robotick
{
#if defined(ROBOTICK_TEST_MODE)

	class TestError : public std_approved::exception
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

	inline void report_error(const char* message, const char* file, int line, const char* func)
	{
		fprintf(stderr, "\033[1;31m[ERROR] %s:%d (%s): %s\033[0m\n", robotick_filename(file), line, func, message);
		throw TestError(message);
	}

#else // === normal (non‑test) mode ===

	inline void report_error(const char* message, const char* file, int line, const char* func)
	{
		ROBOTICK_BREAKPOINT();

		fprintf(stderr, "\033[1;31m[ERROR] %s:%d (%s): %s\033[0m\n", robotick_filename(file), line, func, message);

#if defined(__linux__) || defined(__APPLE__)
		void* callstack[64];
		const int frames = backtrace(callstack, 64);

		fprintf(stderr, "Callstack:\n");

		char** symbols = backtrace_symbols(callstack, frames);

		for (int i = 1; i < frames; ++i) // skip report_error itself
		{
			Dl_info info{};
			const bool dladdr_ok = dladdr(callstack[i], &info) != 0;
			char module_path[512] = {};
			uintptr_t module_base = 0;

			if (dladdr_ok && info.dli_fname && info.dli_fname[0] != '\0')
			{
				strncpy(module_path, info.dli_fname, sizeof(module_path) - 1);
				module_base = reinterpret_cast<uintptr_t>(info.dli_fbase);
			}
			else
			{
				robotick_resolve_module_from_proc_maps(callstack[i], module_path, sizeof(module_path), module_base);
			}

			bool printed_symbol = false;

			if (dladdr_ok && info.dli_sname)
			{
				int status = 0;
				const char* symname = info.dli_sname;
				char* demangled = abi::__cxa_demangle(symname, nullptr, nullptr, &status);
				const char* pretty = (status == 0 && demangled) ? demangled : symname;
				fprintf(stderr, "  [%02d] %s\n", i, pretty);
				if (demangled)
					free(demangled);
				printed_symbol = true;
			}

			if (!printed_symbol)
			{
				if (module_path[0] != '\0')
					fprintf(stderr, "  [%02d] %s @ %p\n", i, module_path, callstack[i]);
				else if (symbols && symbols[i])
					fprintf(stderr, "  [%02d] %s\n", i, symbols[i]);
				else
					fprintf(stderr, "  [%02d] (unresolved @ %p)\n", i, callstack[i]);
			}

			if (module_path[0] != '\0')
			{
				uintptr_t abs_addr = reinterpret_cast<uintptr_t>(callstack[i]);
				uintptr_t rel_addr = module_base ? (abs_addr - module_base) : abs_addr;

				char cmd[512];
				const size_t raw_len = strnlen(module_path, sizeof(module_path));
				const int path_max = static_cast<int>((raw_len < 192) ? raw_len : 192);
				snprintf(
					cmd, sizeof(cmd), "addr2line -e %.*s -f -p 0x%llx 2>/dev/null", path_max, module_path, static_cast<unsigned long long>(rel_addr));
				FILE* fp = popen(cmd, "r");
				if (fp)
				{
					char buf[512];
					if (fgets(buf, sizeof(buf), fp))
					{
						size_t len = strlen(buf);
						if (len && buf[len - 1] == '\n')
							buf[len - 1] = 0;
						fprintf(stderr, "        ↳ %s\n", buf);
					}
					pclose(fp);
				}
			}
		}

		if (symbols)
			free(symbols);
#else
		fprintf(stderr, "(Callstack not supported on this platform)\n");
#endif

		fflush(stderr);
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
		robotick::report_error(__robotick_error_buf, __FILE__, __LINE__, __func__);                                                                  \
	} while (0)

#define ROBOTICK_WARNING(...)                                                                                                                        \
	do                                                                                                                                               \
	{                                                                                                                                                \
		ROBOTICK_BREAKPOINT();                                                                                                                       \
		ROBOTICK_INTERNAL_LOG("WARN", __VA_ARGS__);                                                                                                  \
	} while (0)

#define ROBOTICK_WARNING_IF(cond, ...)                                                                                                               \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!cond)                                                                                                                                   \
			break;                                                                                                                                   \
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

#define ROBOTICK_INFO_IF(cond, ...)                                                                                                                  \
	do                                                                                                                                               \
	{                                                                                                                                                \
		if (!cond)                                                                                                                                   \
			break;                                                                                                                                   \
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
			REQUIRE(string_contains(e.what(), substr_literal));                                                                                      \
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
