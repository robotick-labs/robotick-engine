#pragma once

#if defined(ROBOTICK_PLATFORM_ESP32)
namespace robotick {
inline void setup_exit_handler(void (*handler)()) {
    // No-op or platform-specific implementation if needed
}
} // namespace robotick
#else
#include <csignal>

namespace robotick {

inline void setup_exit_handler(void (*handler)()) {
    std::signal(SIGINT, [](int) { handler(); });
    std::signal(SIGTERM, [](int) { handler(); });
}

} // namespace robotick
#endif