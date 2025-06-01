#pragma once

#include <cstdint>

#if defined(ROBOTICK_PLATFORM_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robotick
{

	inline void sleep_ms(uint32_t ms)
	{
		vTaskDelay(pdMS_TO_TICKS(ms));
	}

	class AtomicFlag
	{
	  public:
		volatile bool flag = false;
		void set() { flag = true; }
		bool is_set() const { return flag; }
	};

} // namespace robotick

#else // Desktop

#include <atomic>
#include <chrono>
#include <thread>

namespace robotick
{

	inline void sleep_ms(uint32_t ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	class AtomicFlag
	{
	  public:
		std::atomic<bool> flag{false};
		void set() { flag.store(true); }
		bool is_set() const { return flag.load(); }
	};

} // namespace robotick

#endif