// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Model_v1.h"

#include "robotick/api_base.h"
#include "robotick/framework/data/DataConnection.h"

#include <cassert>
#include <functional>
#include <set>
#include <stdexcept>

namespace robotick
{
	void Model_v1::connect_remote(const std::string& source_field_path, const std::string& dest_field_path)
	{
		// Expected format: |remote_model_id|field.path

		ROBOTICK_ASSERT_MSG((!source_field_path.empty() && source_field_path[0] != '|'), "Source field paths cannot be empty or remote: %s",
			source_field_path.c_str());

		size_t first_pipe = dest_field_path.find('|', 1);
		if (first_pipe == std::string::npos)
		{
			ROBOTICK_FATAL_EXIT("Invalid remote field format: %s", dest_field_path.c_str());
		}

		const std::string model_id = dest_field_path.substr(1, first_pipe - 1);
		const std::string remote_field_path = dest_field_path.substr(first_pipe + 1);

		auto it = remote_models.find(model_id);
		if (it == remote_models.end())
		{
			ROBOTICK_FATAL_EXIT("Unknown remote model ID: %s", model_id.c_str());
		}

		RemoteModelSeed_v1& remote_model = it->second;

		// Prevent duplicate incoming remote-connections
		if (std::any_of(remote_model.remote_data_connection_seeds.begin(), remote_model.remote_data_connection_seeds.end(),
				[&](const auto& s)
				{
					return s.dest_field_path == remote_field_path;
				}))
		{
			ROBOTICK_FATAL_EXIT(
				"Remote destination field already has an incoming remote-connection: |%s|%s", model_id.c_str(), remote_field_path.c_str());
		}

		// Prevent duplicate incoming local-connections
		if (std::any_of(remote_model.model.data_connection_seeds.begin(), remote_model.model.data_connection_seeds.end(),
				[&](const auto& s)
				{
					return s.dest_field_path == remote_field_path;
				}))
		{
			ROBOTICK_FATAL_EXIT(
				"Remote Destination field already has an incoming local-connection: |%s|%s", model_id.c_str(), remote_field_path.c_str());
		}

		// All good - add remote data-connection
		remote_model.remote_data_connection_seeds.push_back({source_field_path, remote_field_path});
	}

	void Model_v1::connect(const std::string& source_field_path, const std::string& dest_field_path)
	{
		if (source_field_path.empty() || dest_field_path.empty())
			ROBOTICK_FATAL_EXIT("Field paths must be non-empty");

		if (source_field_path == dest_field_path)
			ROBOTICK_FATAL_EXIT("Source and destination field paths are identical: %s", dest_field_path.c_str());

		if (!source_field_path.empty() && source_field_path[0] == '|')
			ROBOTICK_FATAL_EXIT("Source field paths cannot be remote: %s", source_field_path.c_str());

		// Destination field is "remote":
		if (!dest_field_path.empty() && dest_field_path[0] == '|')
		{
			connect_remote(source_field_path, dest_field_path);
			return; // we're done with remote path, skip local below
		}

		if (std::any_of(data_connection_seeds.begin(), data_connection_seeds.end(),
				[&](const auto& s)
				{
					return s.dest_field_path == dest_field_path;
				}))
		{
			ROBOTICK_FATAL_EXIT("Destination field already has an incoming connection: %s", dest_field_path.c_str());
		}

		if (root_workload.is_valid())
			ROBOTICK_FATAL_EXIT("Cannot add connections after root has been set. Model_v1 root must be set last.");

		data_connection_seeds.push_back({source_field_path, dest_field_path});
	}

	void Model_v1::finalize()
	{
		ROBOTICK_ASSERT(root_workload.is_valid() && "Model_v1 root must be set before validation");

		std::function<void(WorkloadHandle_v1, double)> validate_recursively = [&](WorkloadHandle_v1 handle, double parent_tick_rate)
		{
			WorkloadSeed_v1& seed = workload_seeds[handle.index];

			// Inherit tick rate if using TICK_RATE_FROM_PARENT (tick_rate_hz==0.0)
			if (seed.tick_rate_hz == TICK_RATE_FROM_PARENT)
			{
				seed.tick_rate_hz = parent_tick_rate;
			}
			else if (seed.tick_rate_hz > parent_tick_rate)
			{
				ROBOTICK_FATAL_EXIT("Child workload cannot have faster tick rate than parent");
			}
			// Recurse through children
			for (WorkloadHandle_v1 child : seed.children)
			{
				validate_recursively(child, seed.tick_rate_hz);
			}
		};

		// Root can run at any tick rate, but must be explicitly set
		WorkloadSeed_v1& root_seed = workload_seeds[root_workload.index];
		if (root_seed.tick_rate_hz == TICK_RATE_FROM_PARENT)
		{
			ROBOTICK_FATAL_EXIT("Root workload must have an explicit tick rate");
		}

		validate_recursively(root_workload, root_seed.tick_rate_hz);

		// validate data-connections:
		std::set<std::string> connected_inputs;
		for (const DataConnectionSeed_v1& data_connection_seed : data_connection_seeds)
		{
			// (1) only a single incoming connection to any given input
			const bool dest_already_has_connection = connected_inputs.find(data_connection_seed.dest_field_path) != connected_inputs.end();
			if (dest_already_has_connection)
			{
				ROBOTICK_FATAL_EXIT(
					"Data connection error: destination field '%s' already has an incoming connection. Cannot connect from source field "
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

	WorkloadHandle_v1 Model_v1::add(
		const std::string& type, const std::string& name, double tick_rate_hz, const std::map<std::string, std::string>& config)
	{
		if (root_workload.is_valid())
			ROBOTICK_FATAL_EXIT("Cannot add workloads after root has been set. Model_v1 root must be set last.");

		const std::vector<WorkloadHandle_v1> children = {};
		workload_seeds.push_back({type, name, tick_rate_hz, children, config});
		return {static_cast<uint32_t>(workload_seeds.size() - 1)};
	}

	WorkloadHandle_v1 Model_v1::add(const std::string& type, const std::string& name, const std::vector<WorkloadHandle_v1>& children,
		double tick_rate_hz, const std::map<std::string, std::string>& config)
	{
		if (root_workload.is_valid())
			ROBOTICK_FATAL_EXIT("Cannot add workloads after root has been set. Model_v1 root must be set last.");

		for (auto child : children)
		{
			if (!child.is_valid() || child.index >= workload_seeds.size())
				ROBOTICK_FATAL_EXIT("Child handle out of range when adding workload '%s'", name.c_str());
		}

		workload_seeds.push_back({type, name, tick_rate_hz, children, config});
		return {static_cast<uint32_t>(workload_seeds.size() - 1)};
	}

	void Model_v1::add_remote_model(const Model_v1& remote_model, const std::string& model_name, const std::string& comms_channel)
	{
		if (model_name.empty())
		{
			ROBOTICK_FATAL_EXIT("add_remote_model: model_name must not be empty");
		}

		if (remote_model.get_workload_seeds().empty())
		{
			ROBOTICK_FATAL_EXIT("add_remote_model: remote_model must contain at least one workload");
		}

		if (remote_models.find(model_name) != remote_models.end())
		{
			ROBOTICK_FATAL_EXIT("add_remote_model: a remote model with name '%s' already exists", model_name.c_str());
		}

		const auto separator = comms_channel.find(':');
		if (separator == std::string::npos)
		{
			ROBOTICK_FATAL_EXIT("add_remote_model: invalid comms_channel format, expected <mode>:<address>");
		}

		const std::string mode = comms_channel.substr(0, separator);
		const std::string address = comms_channel.substr(separator + 1);

		RemoteModelSeed_v1 seed;
		seed.model_name = model_name;

		if (mode == "ip")
		{
			seed.comms_mode = RemoteModelSeed_v1::Mode::IP;
		}
		else
		{
			ROBOTICK_FATAL_EXIT("add_remote_model: unsupported comms_channel mode: '%s'", mode.c_str());
		}

		seed.comms_channel = address;
		seed.model = remote_model; // deep copy

		remote_models[model_name] = std::move(seed);
	}

	void Model_v1::set_root(WorkloadHandle_v1 handle, const bool auto_finalize)
	{
		root_workload = handle; // no more changes once root has been set, so good time to validate the model...
		if (auto_finalize)
		{
			finalize();
		}
	}

} // namespace robotick
