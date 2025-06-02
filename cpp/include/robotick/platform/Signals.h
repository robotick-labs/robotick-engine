#pragma once

#if defined(ROBOTICK_PLATFORM_ESP32)

namespace robotick
{
	inline void setup_exit_handler(void (*handler)())
	{
		// No-op or platform-specific implementation if needed
	}
} // namespace robotick

#else

#include <csignal>

namespace robotick
{
	namespace
	{
		static void (*g_exit_handler)() = nullptr;

		void signal_trampoline(int)
		{
			if (g_exit_handler)
				g_exit_handler();
		}
	} // namespace

	inline void setup_exit_handler(void (*handler)())
	{
		g_exit_handler = handler;
		std::signal(SIGINT, signal_trampoline);
		std::signal(SIGTERM, signal_trampoline);
	}
} // namespace robotick

#endif
