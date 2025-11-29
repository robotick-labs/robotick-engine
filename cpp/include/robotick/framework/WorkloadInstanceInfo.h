// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/common/FixedVector.h"
#include "robotick/framework/common/HeapVector.h"
#include "robotick/framework/utils/Constants.h"

#include <cstdint>
#include <stddef.h>

namespace robotick
{
	class Engine;
	struct TypeDescriptor;
	struct WorkloadSeed;
	struct WorkloadsBuffer;
	struct WorkloadDescriptor;

	namespace detail
	{
		static inline uint32_t clamp_to_uint32(uint64_t value)
		{
			return (value > UINT32_MAX) ? UINT32_MAX : static_cast<uint32_t>(value);
		}
	} // namespace detail

	using TickDurationWindow = FixedVector<uint32_t, 64>;

	struct WorkloadInstanceStats
	{
		uint32_t last_tick_duration_ns = 0; // (uint32_t can store up to 4.29s of nanoseconds - should be fine for these deltas)
		uint32_t last_time_delta_ns = 0;
		float tick_rate_hz = 0.0f;

		TickDurationWindow duration_window;
		uint32_t window_index = 0;
		uint32_t overrun_count = 0;

		void record_tick_duration_ns(uint32_t duration_ns, uint32_t budget_ns);

		const TickDurationWindow& get_duration_window() const { return duration_window; }
		size_t get_duration_window_index() const { return window_index; }
		size_t get_duration_window_count() const { return duration_window.size(); }

		float get_last_tick_duration_sec() const { return (float)last_tick_duration_ns * 1e-9f; }
		float get_last_time_delta_sec() const { return (float)last_time_delta_ns * 1e-9f; }

		float get_last_tick_duration_ms() const { return (float)last_tick_duration_ns * 1e-6f; }
		float get_last_time_delta_ms() const { return (float)last_time_delta_ns * 1e-6f; }

		// Internal sliding-window instrumentation (not part of the public API):
	};

	struct WorkloadInstanceInfo
	{
		uint8_t* get_ptr(const Engine& engine) const;
		uint8_t* get_ptr(WorkloadsBuffer& workloads_buffer) const;

		// constant once created:
		const WorkloadSeed* seed = nullptr;
		const TypeDescriptor* type = nullptr;
		const WorkloadDescriptor* workload_descriptor = nullptr;
		size_t offset_in_workloads_buffer = OFFSET_UNBOUND;

		HeapVector<const WorkloadInstanceInfo*> children;

		WorkloadInstanceStats* workload_stats = nullptr;
	};

	inline void WorkloadInstanceStats::record_tick_duration_ns(uint32_t duration_ns, uint32_t budget_ns)
	{
		if (budget_ns == 0)
			budget_ns = 1;

		if (duration_ns > budget_ns)
			++overrun_count;

		if (duration_window.size() < duration_window.capacity())
		{
			duration_window.add(duration_ns);
		}
		else
		{
			duration_window[window_index] = duration_ns;
		}

		window_index = (window_index + 1) % (uint32_t)duration_window.capacity();
		last_tick_duration_ns = duration_ns;
	}

} // namespace robotick
