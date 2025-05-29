// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace robotick
{
	struct WorkloadRegistryEntry;

	struct WorkloadInstanceInfo
	{
		// constant once created:
		uint8_t* ptr = nullptr;
		const WorkloadRegistryEntry* type = nullptr;
		std::string unique_name;
		double tick_rate_hz = 0.0;
		std::vector<const WorkloadInstanceInfo*> children;

		// mutable state:
		mutable double last_time_delta = 0.0; // mutable so we can set it during ticking, even when WorkloadInstanceInfo is const
	};
} // namespace robotick