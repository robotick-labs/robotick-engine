// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

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

	inline Thread::~Thread()
	{
		// Task deletes itself
	}

	inline void Thread::join()
	{
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
	}

	inline void Thread::set_priority_high()
	{
	}

	inline void Thread::set_affinity(int)
	{
	}

	inline void Thread::hybrid_sleep_until(std::chrono::steady_clock::time_point target_time)
	{
		using namespace std::chrono_literals;
		constexpr auto safety_margin = 500us;

		while (true)
		{
			auto now = std::chrono::steady_clock::now();
			if (now >= target_time)
				break;

			auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(target_time - now);
			if (remaining > safety_margin)
				vTaskDelay(pdMS_TO_TICKS(remaining.count()));
			else
				for (volatile int i = 0; i < 100;)
				{
					i += 1;
				}
		}
	}

} // namespace robotick
