#pragma once
#include <string>

#if defined(_WIN32)
// Windows 10+ and Visual Studio 2017+
#include <windows.h>
#include <processthreadsapi.h>
#endif  // #if defined(_WIN32)

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
        (void)name;  // suppress unused warning
#endif
    }

    // Platform helpers
    void enable_high_resolution_timing()
    {
#ifdef _WIN32
        timeBeginPeriod(1);
#else
        // No-op on Linux
#endif
    }

    void disable_high_resolution_timing()
    {
#ifdef _WIN32
        timeEndPeriod(1);
#else
        // No-op on Linux
#endif
    }

    void set_thread_priority_high()
    {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#else
        // Optional: use pthread_setschedparam if real-time priority is
        // needed
#endif
    }

    void set_thread_affinity(int core)
    {
#ifdef _WIN32
        DWORD_PTR mask = 1 << core;
        SetThreadAffinityMask(GetCurrentThread(), mask);
#else
        (void)core;  // suppress unused parameter warning
                     // Optional: use pthread_setaffinity_np
#endif
    }

}  // namespace robotick
