// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Model.h"

#include "robotick/api.h"
#include "robotick/framework/data/DataConnection.h"

#include <cassert>
#include <functional>
#include <set>
#include <stdexcept>

namespace robotick
{

	void Model::connect(const std::string& source_field_path, const std::string& dest_field_path)
	{
		if (source_field_path.empty() || dest_field_path.empty())
			ROBOTICK_ERROR("Field paths must be non-empty");

		if (source_field_path == dest_field_path)
			ROBOTICK_ERROR("Source and destination field paths are identical: %s", dest_field_path.c_str());

		if (std::any_of(data_connection_seeds.begin(), data_connection_seeds.end(),
				[&](const auto& s)
				{
					return s.dest_field_path == dest_field_path;
				}))
		{
			ROBOTICK_ERROR("Destination field already has an incoming connection: %s", dest_field_path.c_str());
		}

		if (root_workload.is_valid())
			ROBOTICK_ERROR("Cannot add connections after root has been set. Model root must be set last.");

		data_connection_seeds.push_back({source_field_path, dest_field_path});
	}

	void Model::finalize()
	{
		ROBOTICK_ASSERT(root_workload.is_valid() && "Model root must be set before validation");

		std::function<void(WorkloadHandle, double)> validate_recursively = [&](WorkloadHandle handle, double parent_tick_rate)
		{
			WorkloadSeed& seed = workload_seeds[handle.index];

			// Inherit tick rate if using TICK_RATE_FROM_PARENT (tick_rate_hz==0.0)
			if (seed.tick_rate_hz == TICK_RATE_FROM_PARENT)
			{
				seed.tick_rate_hz = parent_tick_rate;
			}
			else if (seed.tick_rate_hz > parent_tick_rate)
			{
				ROBOTICK_ERROR("Child workload cannot have faster tick rate than parent");
			}
			// Recurse through children
			for (WorkloadHandle child : seed.children)
			{
				validate_recursively(child, seed.tick_rate_hz);
			}
		};

		// Root can run at any tick rate, but must be explicitly set
		WorkloadSeed& root_seed = workload_seeds[root_workload.index];
		if (root_seed.tick_rate_hz == TICK_RATE_FROM_PARENT)
		{
			ROBOTICK_ERROR("Root workload must have an explicit tick rate");
		}

		validate_recursively(root_workload, root_seed.tick_rate_hz);

		// validate data-connections:
		std::set<std::string> connected_inputs;
		for (const DataConnectionSeed& data_connection_seed : data_connection_seeds)
		{
			// (1) only a single incoming connection to any given input
			const bool dest_already_has_connection = connected_inputs.find(data_connection_seed.dest_field_path) != connected_inputs.end();
			if (dest_already_has_connection)
			{
				ROBOTICK_ERROR("Data connection error: destination field '%s' already has an incoming connection. Cannot connect from source field "
							   "'%s'.\nEach input field may only be connected once.",
					data_connection_seed.dest_field_path.c_str(), data_connection_seed.source_field_path.c_str());
			}
			else
			{
				// not found - add it
				connected_inputs.emplace(data_connection_seed.dest_field_path);
			}

			// (2) we do further validation at engine.load() time and before - we may wish to push some earlier to this stage - if so it can go here
			// ...
		}
	}

} // namespace robotick
