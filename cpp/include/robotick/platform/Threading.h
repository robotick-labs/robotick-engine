// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#if defined(ROBOTICK_PLATFORM_ESP32)
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#else
#include <thread>
#endif

namespace robotick
{
	class Thread
	{
	  public:
		using EntryPoint = void (*)(void*);

		Thread() = default;
		Thread(EntryPoint fn, void* arg, const std::string& name = "", int core = -1, int stack_size = 8192, int priority = 1);
		~Thread();

		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;

		Thread(Thread&& other) noexcept = default;
		Thread& operator=(Thread&& other) noexcept = default;

		bool is_joining_supported() const; // call this before either is_joinable() or join() - join() will call ROBOTICK_FATAL_EXIT if not supported
		bool is_joinable() const;
		void join();

		static void yield();
		static void sleep_ms(uint32_t ms);
		static void hybrid_sleep_until(std::chrono::steady_clock::time_point target_time);

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

	class AtomicFlag
	{
	  public:
		explicit AtomicFlag(bool initial = false)
			: flag(initial)
		{
		}

		void set(bool value = true) { flag.store(value); }
		bool is_set() const { return flag.load(); }

	  private:
		std::atomic<bool> flag{false};
	};

} // namespace robotick

// Platform-specific implementation
#if defined(ROBOTICK_PLATFORM_ESP32)
#include "robotick/platform/esp32/Threading_esp32.inl"
#elif defined(ROBOTICK_PLATFORM_DESKTOP)
#include "robotick/platform/desktop/Threading_desktop.inl"
#else
#error "No Threading implementation for this platform – define a platform macro or add a generic fallback"
#endif
