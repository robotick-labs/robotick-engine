// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/WorkloadInstanceInfo.h"

#include "robotick/api_base.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeDescriptor.h"
#include "robotick/framework/registry/TypeMacros.h"

namespace robotick
{
	ROBOTICK_REGISTER_FIXED_VECTOR(TickDurationWindow, uint32_t);
	ROBOTICK_REGISTER_FIXED_VECTOR(TickDeltaWindow, uint32_t);

	ROBOTICK_REGISTER_STRUCT_BEGIN(WorkloadInstanceStats)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, uint32_t, last_tick_duration_ns)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, uint32_t, last_time_delta_ns)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, uint64_t, tick_count)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, float, tick_rate_hz)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, TickDurationWindow, duration_window)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, TickDeltaWindow, delta_window)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, uint32_t, duration_window_index)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, uint32_t, delta_window_index)
	ROBOTICK_STRUCT_FIELD(WorkloadInstanceStats, uint32_t, overrun_count)
	ROBOTICK_REGISTER_STRUCT_END(WorkloadInstanceStats)

	uint8_t* WorkloadInstanceInfo::get_ptr(const Engine& engine) const
	{
		return get_ptr(const_cast<WorkloadsBuffer&>(engine.get_workloads_buffer()));
	}

	uint8_t* WorkloadInstanceInfo::get_ptr(WorkloadsBuffer& workloads_buffer) const
	{
		ROBOTICK_ASSERT(this->offset_in_workloads_buffer != OFFSET_UNBOUND && "Workload object offset should have been set by now");

		uint8_t* ptr = workloads_buffer.raw_ptr() + this->offset_in_workloads_buffer;

		ROBOTICK_ASSERT(workloads_buffer.contains_object(ptr, this->type->size) &&
						"WorkloadInstanceInfo computed should be within the workloads-buffer provided");

		return ptr;
	}

} // namespace robotick
