// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/WorkloadInstanceInfo.h"

#include "robotick/api_base.h"
#include "robotick/framework/Engine.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/TypeDescriptor.h"

namespace robotick
{

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
