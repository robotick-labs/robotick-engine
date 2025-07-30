// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace robotick
{
	struct WorkloadInstanceStats;

	struct TickInfo
	{
		float delta_time = 0.0;	  // Time since last tick [seconds]
		float time_now = 0.0;	  // Time since engine start [seconds] — derived from time_now_ns each tick (so no floating-point drift)
		uint64_t time_now_ns = 0; // Monotonic time since engine start [nanoseconds] — resolution and drift depend purely on platform timer quality
		uint64_t tick_count = 0;  // Number of ticks since engine start

		const WorkloadInstanceStats* workload_stats = nullptr;
	};

	static const TickInfo TICK_INFO_FIRST_1MS_1KHZ = {
		0.001f,	   // delta_time (1 millisecond)
		0.001f,	   // time_now (same as delta_time for first tick)
		1'000'000, // time_now_ns (1 ms in nanoseconds)
		1		   // tick_count
	};

	static const TickInfo TICK_INFO_FIRST_10MS_100HZ = {
		0.01f,		// delta_time (10 milliseconds)
		0.01f,		// time_now (same as delta_time for first tick)
		10'000'000, // time_now_ns (10 ms in nanoseconds)
		1			// tick_count
	};

	static const TickInfo TICK_INFO_FIRST_100MS_10HZ = {
		0.1f,		 // delta_time (100 milliseconds)
		0.1f,		 // time_now (same as delta_time for first tick)
		100'000'000, // time_now_ns (10 ms in nanoseconds)
		1			 // tick_count
	};

} // namespace robotick