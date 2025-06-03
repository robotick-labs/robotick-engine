// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api.h"

#include <chrono>
#include <cstdint>
#include <string>

#if defined(ROBOTICK_PLATFORM_ESP32)
#include "esp_atomic.h" // <atomic> alternatives for ESP-IDF
#include "esp_idf_version.h"
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
		Thread(EntryPoint fn, void* arg, const std::string& name = "", int core = -1, int stack_size = 4096, int priority = 1);
		~Thread();

		// Non-copyable
		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;

		// Move-enabled
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

#if defined(ROBOTICK_PLATFORM_ESP32)
	class AtomicFlag
	{
	  public:
		explicit AtomicFlag(bool initial = false) { atomic_store_u32(&flag, initial ? 1 : 0); }

		void set(bool value = true) { atomic_store_u32(&flag, value ? 1 : 0); }

		bool is_set() const { return atomic_load_u32(&flag) != 0; }

	  private:
		atomic_uint32_t flag;
	};
#else
	// std::atomic fallback for desktop
	class AtomicFlag
	{
	  public:
		explicit AtomicFlag(bool initial = false) : flag(initial) {}

		void set(bool value = true) { flag.store(value); }
		bool is_set() const { return flag.load(); }

	  private:
		std::atomic<bool> flag{false};
	};
#endif

} // namespace robotick

// Platform-specific implementation
#if defined(ROBOTICK_PLATFORM_ESP32)
#include "robotick/platform/detail/Threading_esp32.inl"
#else
#include "robotick/platform/detail/Threading_desktop.inl"
#endif
