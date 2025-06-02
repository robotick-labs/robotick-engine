#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace robotick
{
	struct ThreadSyncGroup::Impl
	{
		SemaphoreHandle_t mutex;
		SemaphoreHandle_t cond;
	};

	inline ThreadSyncGroup::ThreadSyncGroup()
	{
		impl = new Impl;
		impl->mutex = xSemaphoreCreateMutex();
		impl->cond = xSemaphoreCreateBinary();
	}

	inline ThreadSyncGroup::~ThreadSyncGroup()
	{
		vSemaphoreDelete(impl->mutex);
		vSemaphoreDelete(impl->cond);
		delete impl;
	}

	inline void ThreadSyncGroup::notify_all()
	{
		xSemaphoreGive(impl->cond);
	}

	inline bool ThreadSyncGroup::wait_for_tick(uint32_t& last_tick, uint32_t current_tick)
	{
		while (last_tick == current_tick)
		{
			xSemaphoreTake(impl->cond, portMAX_DELAY);
		}
		last_tick = current_tick;
		return true;
	}

	inline void ThreadSyncGroup::run_loop(const std::function<void()>& on_tick, const AtomicFlag& exit_flag)
	{
		uint32_t tick_counter = 0;
		while (!exit_flag.is_set())
		{
			xSemaphoreTake(impl->mutex, portMAX_DELAY);
			on_tick();
			tick_counter++;
			xSemaphoreGive(impl->mutex);
			notify_all();  // wake up any waiters
			vTaskDelay(1); // or use a configurable tick delay
		}
	}
} // namespace robotick
