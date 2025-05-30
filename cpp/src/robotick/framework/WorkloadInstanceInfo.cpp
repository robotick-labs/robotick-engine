// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/WorkloadInstanceInfo.h"

#include "robotick/framework/Engine.h"
#include "robotick/framework/data/WorkloadsBuffer.h"

#include <cassert>

namespace robotick
{

	uint8_t* WorkloadInstanceInfo::get_ptr(const Engine& engine) const
	{
		return get_ptr(const_cast<WorkloadsBuffer&>(engine.get_workloads_buffer()));
	}

	uint8_t* WorkloadInstanceInfo::get_ptr(WorkloadsBuffer& workloads_buffer) const
	{
		assert(this->offset != OFFSET_UNBOUND);

		return workloads_buffer.raw_ptr() + this->offset;
	}

} // namespace robotick
