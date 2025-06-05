// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace robotick
{

	struct TickInfo
	{
		double delta_time = 0.0;  // Time since last tick [seconds]
		double time_now = 0.0;	  // Time since engine start [seconds] — derived from time_now_ns each tick (so no floating-point drift)
		uint64_t time_now_ns = 0; // Monotonic time since engine start [nanoseconds] — resolution and drift depend purely on platform timer quality
		uint64_t tick_count = 0;  // Number of ticks since engine start
	};

	static const TickInfo TICK_INFO_FIRST_1MS_1KHZ = {
		0.001,	   // delta_time (1 millisecond)
		0.001,	   // time_now (same as delta_time for first tick)
		1'000'000, // time_now_ns (1 ms in nanoseconds)
		1		   // tick_count
	};

	static const TickInfo TICK_INFO_FIRST_10MS_100HZ = {
		0.01,		// delta_time (10 milliseconds)
		0.01,		// time_now (same as delta_time for first tick)
		10'000'000, // time_now_ns (10 ms in nanoseconds)
		1			// tick_count
	};

	static const TickInfo TICK_INFO_FIRST_100MS_10HZ = {
		0.1,		 // delta_time (100 milliseconds)
		0.1,		 // time_now (same as delta_time for first tick)
		100'000'000, // time_now_ns (10 ms in nanoseconds)
		1			 // tick_count
	};

} // namespace robotick