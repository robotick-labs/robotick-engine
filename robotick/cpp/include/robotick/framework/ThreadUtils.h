#pragma once
#include <string>

#if defined(_WIN32)
// Windows 10+ and Visual Studio 2017+
#include <windows.h>
#include <processthreadsapi.h>
#endif // #if defined(_WIN32)

inline void set_thread_name(const std::string &name)
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
