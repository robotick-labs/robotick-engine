// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/api_base.h"
#include "robotick/framework/time/Clock.h"
#include <cstdint>
#include <thread>

namespace robotick
{
	enum class HybridSleepMode
	{
		// Lowest CPU overhead. Suitable for workloads where a little wake lateness is acceptable,
		// such as robot models doing background perception, mapping, or other non-tight-loop work.
		Eco,
		// Balanced default. Good for most desktop robot models where we want stable pacing
		// without burning CPU in a long busy-wait near the deadline.
		Normal,
		// Highest timing precision. Intended for workloads where the last part of the frame budget
		// matters enough to justify a short final spin, such as tightly paced simulation/control loops.
		Accurate
	};

	struct HybridSleepStats
	{
		HybridSleepMode mode = HybridSleepMode::Normal;

		uint64_t requested_wait_ns = 0;
		uint64_t coarse_sleep_ns = 0;
		uint64_t yield_phase_ns = 0;
		uint64_t spin_phase_ns = 0;
		uint64_t total_wait_ns = 0;
		uint64_t wake_lateness_ns = 0;

		uint32_t yield_iterations = 0;
		uint32_t spin_iterations = 0;
	};

	class Thread
	{
	  public:
		using ThreadId = uintptr_t;

		using EntryPoint = void (*)(void*);

		Thread() = default;
		Thread(EntryPoint fn, void* arg, const char* name = "", int core = -1, int stack_size = 12288, int priority = 1);
		~Thread();

		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;

		Thread(Thread&& other) noexcept = default;
		Thread& operator=(Thread&& other) noexcept = default;

		bool is_joining_supported() const; // call this before either is_joinable() or join() - join() will call ROBOTICK_FATAL_EXIT if not supported
		bool is_joinable() const;
		void join();

		static ThreadId get_current_thread_id();
		static uint32_t get_hardware_concurrency();
		static void yield();
		static void sleep_ms(uint32_t ms);
		static void hybrid_sleep_until(
			Clock::time_point target_time, HybridSleepMode mode = HybridSleepMode::Normal, HybridSleepStats* out_stats = nullptr);

	  protected:
		static void set_name(const char* name);
		static void set_priority_high();
		static void set_affinity(int core);

	  private:
#if defined(ROBOTICK_PLATFORM_ESP32S3)
		void* handle = nullptr;
#else
		std_approved::thread thread;
#endif
	};

} // namespace robotick

// Platform-specific implementation
#if defined(ROBOTICK_PLATFORM_ESP32S3)
#include "robotick/framework/backends/esp32/Thread_esp32.inl"
#elif defined(ROBOTICK_PLATFORM_DESKTOP)
#include "robotick/framework/backends/desktop/Thread_desktop.inl"
#else
#error "No Threading implementation for this platform – define a platform macro or add a generic fallback"
#endif
