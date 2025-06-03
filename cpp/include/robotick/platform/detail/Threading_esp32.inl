// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "esp_task_wdt.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

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

	inline Thread::Thread(EntryPoint fn, void* arg, const std::string& name, int core, int stack_size, int priority)
	{
		auto* ctx = new TaskContext{fn, arg};
		BaseType_t result;

		if (core >= 0)
		{
			result = xTaskCreatePinnedToCore(task_entry, name.c_str(), stack_size, ctx, priority, reinterpret_cast<TaskHandle_t*>(&handle), core);
		}
		else
		{
			result = xTaskCreate(task_entry, name.c_str(), stack_size, ctx, priority, reinterpret_cast<TaskHandle_t*>(&handle));
		}

		if (result != pdPASS)
		{
			delete ctx;
			handle = nullptr;
		}
	}

	// Move constructor
	inline Thread::Thread(Thread&& other) noexcept
	{
		handle = other.handle;
		other.handle = nullptr;
	}

	// Move assignment
	inline Thread& Thread::operator=(Thread&& other) noexcept
	{
		if (this != &other)
		{
			handle = other.handle;
			other.handle = nullptr;
		}
		return *this;
	}

	inline Thread::~Thread()
	{
		// Task deletes itself using vTaskDelete(nullptr)
	}

	inline void Thread::join()
	{
		// Not supported on ESP32 — FreeRTOS tasks delete themselves
	}

	inline bool Thread::is_joinable() const
	{
		return false;
	}

	inline void Thread::sleep_ms(uint32_t ms)
	{
		vTaskDelay(pdMS_TO_TICKS(ms));
	}

	inline void Thread::set_name(const std::string&)
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

	inline void Thread::hybrid_sleep_until(std::chrono::steady_clock::time_point target_time)
	{
		using namespace std::chrono;

		constexpr auto coarse_threshold_us = 2000; // 2 ms
		constexpr int watchdog_yield_interval = 500;
		int spin_counter = 0;

		// Convert target_time to absolute time in microseconds
		int64_t target_us = duration_cast<microseconds>(target_time.time_since_epoch()).count();

		esp_task_wdt_reset();

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
				taskYIELD();		  // Allow IDLE tasks to run
				esp_task_wdt_reset(); // Pet the watchdog manually
			}
		}
	}

} // namespace robotick
