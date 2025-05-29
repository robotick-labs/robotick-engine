// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Model.h"

#include <cassert>
#include <functional>
#include <set>
#include <stdexcept>

namespace robotick
{
	void Model::finalize()
	{
		assert(root_workload.is_valid() && "Model root must be set before validation");

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
				throw std::runtime_error("Child workload cannot have faster tick rate than parent");
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
			throw std::runtime_error("Root workload must have an explicit tick rate");
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
				throw std::runtime_error("Data connection error: destination field '" + data_connection_seed.dest_field_path +
										 "' already has an incoming connection. Cannot connect from source field '" +
										 data_connection_seed.source_field_path + "'.\nEach input field may only be connected once.");
			}

			// (2) we do further validation at engine.load() time and before - we may wish to push some earlier to this stage - if so it can go here
			// ...
		}
	}

} // namespace robotick
