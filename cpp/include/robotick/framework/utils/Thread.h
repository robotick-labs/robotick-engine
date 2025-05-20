#pragma once
#include <chrono>
#include <string>
#include <thread>

#if defined(_WIN32)
// Windows 10+ and Visual Studio 2017+
#include <processthreadsapi.h>
#include <windows.h>
#endif // #if defined(_WIN32)

namespace robotick
{

	inline void set_thread_name(const std::string& name)
	{
#if defined(_WIN32)
		// Windows 10+ and Visual Studio 2017+
		std::wstring wname(name.begin(), name.end());
		SetThreadDescription(GetCurrentThread(), wname.c_str());

#elif defined(__APPLE__)
		// macOS: pthread_setname_np sets *current* thread name
		pthread_setname_np(name.c_str());

#elif defined(__linux__)
		// Linux: pthread_setname_np(thread, name) or self
		pthread_setname_np(pthread_self(), name.c_str());

#else
		// Unsupported platform
		(void)name; // suppress unused warning
#endif
	}

	// Platform helpers
	inline void enable_high_resolution_timing()
	{
#ifdef _WIN32
		timeBeginPeriod(1);
#else
		// No-op on Linux
#endif
	}

	inline void disable_high_resolution_timing()
	{
#ifdef _WIN32
		timeEndPeriod(1);
#else
		// No-op on Linux
#endif
	}

	inline void set_thread_priority_high()
	{
#ifdef _WIN32
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#else
		// Optional: use pthread_setschedparam if real-time priority is
		// needed
#endif
	}

	inline void set_thread_affinity(int core)
	{
#ifdef _WIN32
		DWORD_PTR mask = 1 << core;
		SetThreadAffinityMask(GetCurrentThread(), mask);
#else
		(void)core; // suppress unused parameter warning
					// Optional: use pthread_setaffinity_np
#endif
	}

	inline void hybrid_sleep_until(std::chrono::steady_clock::time_point target_time)
	{
		using namespace std::chrono;
		constexpr auto coarse_margin = 500us;
		constexpr auto coarse_step = 100us;

		auto now = steady_clock::now();
		while (now < target_time - coarse_margin)
		{
			std::this_thread::sleep_for(coarse_step);
			now = steady_clock::now();
		}
		while (steady_clock::now() < target_time)
		{
		}
	}
} // namespace robotick
