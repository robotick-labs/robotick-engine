// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

namespace robotick
{
	struct WorkloadRegistryEntry;

	struct WorkloadInstanceInfo
	{
		uint8_t* ptr = nullptr;
		const WorkloadRegistryEntry* type = nullptr;
		std::string unique_name;
		double tick_rate_hz = 0.0;
		std::vector<const WorkloadInstanceInfo*> children;
	};
} // namespace robotick