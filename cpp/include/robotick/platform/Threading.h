// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace robotick
{
	class Thread
	{
	  public:
		using EntryPoint = void (*)(void*);

		Thread() = default;
		Thread(EntryPoint fn, void* arg, const std::string& name = "", int core = -1, int stack_size = 4096, int priority = 1);
		~Thread();

		void join(); // Desktop only (no-op on ESP32)
		bool is_joinable() const;

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
#if defined(ROBOTICK_PLATFORM_ESP32)
		volatile bool flag = false;
		void set() { flag = true; }
		bool is_set() const { return flag; }
#else
		std::atomic<bool> flag{false};
		void set() { flag.store(true); }
		bool is_set() const { return flag.load(); }
#endif
	};
} // namespace robotick

// Platform-specific implementation
#if defined(ROBOTICK_PLATFORM_ESP32)
#include "robotick/platform/detail/Threading_esp32.inl"
#else
#include "robotick/platform/detail/Threading_desktop.inl"
#endif
