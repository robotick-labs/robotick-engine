// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/utils/Constants.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace robotick
{
	class Engine;
	struct TypeDescriptor;
	struct WorkloadSeed;
	struct WorkloadsBuffer;
	struct WorkloadDescriptor;

	struct WorkloadInstanceStats
	{
		uint32_t last_tick_duration_ns{0}; // (uint32_t can store up to 4.29s of nanoseconds - should be fine for these deltas)
		uint32_t last_time_delta_ns{0};
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

		// mutable state:
		mutable WorkloadInstanceStats mutable_stats;
		// ^- mutable so we can set it during ticking, even when WorkloadInstanceInfo is const - e.g. for stats
	};
} // namespace robotick