// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "robotick/framework/utils/Constants.h"
#include <cstdint>
#include <string>
#include <vector>

namespace robotick
{
	class Engine;
	struct WorkloadRegistryEntry;
	struct WorkloadsBuffer;

	struct WorkloadInstanceStats
	{
		double last_tick_duration = 0.0;
		double last_time_delta = 0.0;
	};

	struct WorkloadInstanceInfo
	{
		uint8_t* get_ptr(const Engine& engine) const;
		uint8_t* get_ptr(WorkloadsBuffer& workloads_buffer) const;

		// constant once created:
		size_t offset = OFFSET_UNBOUND;
		const WorkloadRegistryEntry* type = nullptr;
		std::string unique_name;
		double tick_rate_hz = 0.0;
		std::vector<const WorkloadInstanceInfo*> children;

		// mutable state:
		mutable WorkloadInstanceStats mutable_stats;
		// ^- mutable so we can set it during ticking, even when WorkloadInstanceInfo is const - e.g. for stats
	};
} // namespace robotick