// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace robotick
{
	struct EngineClockInfo
	{
		float time_now = 0.0f;		  // sec (engine monotonic)
		uint64_t time_now_ns = 0;	  // ns (engine monotonic)
		uint64_t tick_count = 0;	  // engine ticks since start
		float tick_rate_hz = 0.0f;	  // configured nominal rate
		float dt_seconds_last = 0.0f; // last tick delta
	};

	struct EngineInfo
	{
		EngineClockInfo clock;
	};
} // namespace robotick
