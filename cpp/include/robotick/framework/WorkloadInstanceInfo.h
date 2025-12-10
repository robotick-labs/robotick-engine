// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/containers/FixedVector.h"
#include "robotick/framework/containers/HeapVector.h"
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

	using TickDurationWindow = FixedVector<uint32_t, 64>;
	using TickDeltaWindow = FixedVector<uint32_t, 64>;

	struct WorkloadInstanceStats
	{
		// Stats travel with each workload instance inside WorkloadsBuffer so telemetry can be read without locks.
		uint32_t last_tick_duration_ns = 0; // (uint32_t can store up to 4.29s of nanoseconds - should be fine for these deltas)
		uint32_t last_time_delta_ns = 0;
		float tick_rate_hz = 0.0f;

		// Sliding windows live inline so we never touch the heap while sampling ticks.
		TickDurationWindow duration_window;
		TickDeltaWindow delta_window;
		uint32_t duration_window_index = 0;
		uint32_t delta_window_index = 0;
		uint32_t overrun_count = 0;

		void record_tick_sample(uint32_t duration_ns, uint32_t delta_ns, uint32_t budget_ns);

		float get_last_tick_duration_sec() const { return (float)last_tick_duration_ns * 1e-9f; }
		float get_last_time_delta_sec() const { return (float)last_time_delta_ns * 1e-9f; }

		float get_last_tick_duration_ms() const { return (float)last_tick_duration_ns * 1e-6f; }
		float get_last_time_delta_ms() const { return (float)last_time_delta_ns * 1e-6f; }

	  private:
		// Internal sliding-window instrumentation (not part of the public API):
		const TickDurationWindow& get_duration_window() const { return duration_window; }
		const TickDeltaWindow& get_delta_window() const { return delta_window; }
		uint32_t get_duration_window_index() const { return duration_window_index; }
		uint32_t get_delta_window_index() const { return delta_window_index; }
		size_t get_duration_window_count() const { return duration_window.size(); }
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

	inline void WorkloadInstanceStats::record_tick_sample(uint32_t duration_ns, uint32_t delta_ns, uint32_t budget_ns)
	{
		if (budget_ns == 0)
			budget_ns = 1;

		// Track whether the workload exceeded its budget; the stats object owns this monotonic counter
		// for the lifetime of the workload instance (placement-new in WorkloadsBuffer keeps it alive).
		if (duration_ns > budget_ns)
			++overrun_count;

		const auto append_sample = [](auto& window, uint32_t& index, uint32_t sample)
		{
			if (window.size() < window.capacity())
			{
				window.add(sample);
			}
			else
			{
				window[index] = sample;
			}
			index = (index + 1) % (uint32_t)window.capacity();
		};

		// Maintain circular windows of recent tick durations and actual intervals.
		append_sample(duration_window, duration_window_index, duration_ns);
		append_sample(delta_window, delta_window_index, delta_ns);

		last_tick_duration_ns = duration_ns;
		last_time_delta_ns = delta_ns;
	}

} // namespace robotick
