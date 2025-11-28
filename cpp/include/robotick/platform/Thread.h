// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/platform/Clock.h"

#include <string>
#include <thread>

namespace robotick
{
	class Thread
	{
	  public:
		using ThreadId = uintptr_t;

		using EntryPoint = void (*)(void*);

		Thread() = default;
		Thread(EntryPoint fn, void* arg, const std::string& name = "", int core = -1, int stack_size = 12288, int priority = 1);
		~Thread();

		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;

		Thread(Thread&& other) noexcept = default;
		Thread& operator=(Thread&& other) noexcept = default;

		bool is_joining_supported() const; // call this before either is_joinable() or join() - join() will call ROBOTICK_FATAL_EXIT if not supported
		bool is_joinable() const;
		void join();

		static ThreadId get_current_thread_id();
		static void yield();
		static void sleep_ms(uint32_t ms);
		static void hybrid_sleep_until(Clock::time_point target_time);

	  protected:
		static void set_name(const std::string& name);
		static void set_priority_high();
		static void set_affinity(int core);

	  private:
#if defined(ROBOTICK_PLATFORM_ESP32)
		void* handle = nullptr;
#else
		std::thread thread;
#endif
	};

} // namespace robotick

// Platform-specific implementation
#if defined(ROBOTICK_PLATFORM_ESP32)
#include "robotick/platform/esp32/Thread_esp32.inl"
#elif defined(ROBOTICK_PLATFORM_DESKTOP)
#include "robotick/platform/desktop/Thread_desktop.inl"
#else
#error "No Threading implementation for this platform – define a platform macro or add a generic fallback"
#endif
