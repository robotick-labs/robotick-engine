// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "esp_task_wdt.h"
#include "esp_timer.h"

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>

namespace robotick
{

	namespace
	{
		struct TaskContext
		{
			Thread::EntryPoint fn;
			void* arg;
		};

		inline void task_entry(void* param)
		{
			TaskContext ctx = *static_cast<TaskContext*>(param);
			delete static_cast<TaskContext*>(param);
			ctx.fn(ctx.arg);
			vTaskDelete(nullptr); // auto-delete when done
		}
	} // namespace

	inline Thread::Thread(EntryPoint fn, void* arg, const char* name, int core, int stack_size, int priority)
	{
		auto* ctx = new TaskContext{fn, arg};
		BaseType_t result;
		const char* task_name = (name && name[0]) ? name : "robotick-task";

		if (core >= 0)
		{
			result = xTaskCreatePinnedToCore(task_entry, task_name, stack_size, ctx, priority, reinterpret_cast<TaskHandle_t*>(&handle), core);
		}
		else
		{
			result = xTaskCreate(task_entry, task_name, stack_size, ctx, priority, reinterpret_cast<TaskHandle_t*>(&handle));
		}

		if (result != pdPASS)
		{
			delete ctx;
			handle = nullptr;
		}
	}

	inline Thread::~Thread()
	{
		// Task deletes itself using vTaskDelete(nullptr)
	}

	inline bool Thread::is_joining_supported() const
	{
		return false; // this platform does NOT support joining threads - we use tasks which delete themselves
	}

	inline bool Thread::is_joinable() const
	{
		return false;
	}

	inline void Thread::join()
	{
		ROBOTICK_FATAL_EXIT("Thread::join() not supported on ESP32/FreeRTOS - tasks delete themselves");
	}

	inline void Thread::sleep_ms(uint32_t ms)
	{
		vTaskDelay(pdMS_TO_TICKS(ms));
	}

	inline void Thread::set_name(const char*)
	{
		// Optional: could use vTaskSetTaskName if needed
	}

	inline void Thread::set_priority_high()
	{
		// Optional: could elevate task priority here if needed
	}

	inline void Thread::set_affinity(int)
	{
		// Already handled in constructor (xTaskCreatePinnedToCore)
	}

	inline void Thread::yield()
	{
		taskYIELD(); // explicit, zero-latency yield
	}

	inline void Thread::hybrid_sleep_until(Clock::time_point target_time, HybridSleepMode mode, HybridSleepStats* out_stats)
	{
		if (out_stats != nullptr)
		{
			*out_stats = HybridSleepStats{};
			out_stats->mode = mode;
			const Clock::time_point now = Clock::now();
			const int64_t requested_wait_ns = Clock::to_nanoseconds(target_time - now).count();
			out_stats->requested_wait_ns = (requested_wait_ns > 0) ? static_cast<uint64_t>(requested_wait_ns) : 0;
		}

		constexpr auto coarse_threshold_us = 2000; // 2 ms
		constexpr int watchdog_yield_interval = 500;
		int spin_counter = 0;

		// Convert target_time to absolute time in microseconds
		const Clock::duration duration_until = target_time.time_since_epoch();
		const Clock::nanoseconds target_ns = Clock::to_nanoseconds(duration_until);
		int64_t target_us = target_ns.count() / 1000;

		// esp_task_wdt_reset();

		while (true)
		{
			int64_t now_us = esp_timer_get_time();
			int64_t remaining_us = target_us - now_us;

			if (remaining_us <= 0)
				break;

			if (remaining_us > coarse_threshold_us)
			{
				// Sleep in ~50us chunks to avoid CPU hogging
				esp_rom_delay_us(50);
			}

			// Tight spin, yield periodically to prevent WDT issues
			if (++spin_counter % watchdog_yield_interval == 0)
			{
				taskYIELD(); // Allow IDLE tasks to run
			}
		}
	}

	inline Thread::ThreadId Thread::get_current_thread_id()
	{
		return reinterpret_cast<ThreadId>(xTaskGetCurrentTaskHandle());
	}

	inline uint32_t Thread::get_hardware_concurrency()
	{
		// Prefer the IDF-configured core count when available; fall back to single-core.
#ifdef CONFIG_FREERTOS_NUMBER_OF_CORES
		return CONFIG_FREERTOS_NUMBER_OF_CORES;
#else
		return 1;
#endif
	}

} // namespace robotick
