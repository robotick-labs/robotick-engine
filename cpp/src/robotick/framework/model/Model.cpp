// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/model/Model.h"

#include "robotick/api_base.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/RemoteModelSeed.h"
#include "robotick/framework/model/WorkloadSeed.h"
#include "robotick/framework/strings/StringUtils.h"

namespace robotick
{

	namespace
	{
		size_t count_char(const char* begin, char ch)
		{
			size_t count = 0;
			for (const char* cur = begin; *cur != '\0'; ++cur)
			{
				if (*cur == ch)
				{
					count++;
				}
			}
			return count;
		}
	} // namespace

	void Model::use_workload_seeds(const WorkloadSeed* const* all_workloads, size_t num_workloads)
	{
		const WorkloadSeed** mutable_ptr = const_cast<const WorkloadSeed**>(all_workloads);
		workload_seeds = ArrayView<const WorkloadSeed*>(mutable_ptr, num_workloads);

		// validate workloads
		for (const WorkloadSeed* workload_seed : workload_seeds)
		{
			if (!TypeRegistry::get().find_by_id(workload_seed->type_id))
				ROBOTICK_FATAL_EXIT("Unable to find workload type '%s'", workload_seed->type_id.get_debug_name());
		}
	}

	void Model::use_data_connection_seeds(const DataConnectionSeed* const* in_connections, size_t num_connections)
	{
		const DataConnectionSeed** mutable_ptr = const_cast<const DataConnectionSeed**>(in_connections);
		data_connection_seeds = ArrayView<const DataConnectionSeed*>(mutable_ptr, num_connections);
	}

	void Model::use_remote_models(const RemoteModelSeed* const* in_remote_model_seeds, size_t num_remote_model_seeds)
	{
		const RemoteModelSeed** mutable_ptr = const_cast<const RemoteModelSeed**>(in_remote_model_seeds);
		remote_models = ArrayView<const RemoteModelSeed*>(mutable_ptr, num_remote_model_seeds);
	}

	void Model::set_root_workload(const WorkloadSeed& root, bool auto_finalize)
	{
		root_workload = &root;

		if (auto_finalize)
		{
			finalize();
		}
	}

	void Model::set_telemetry_port(const uint16_t in_telemetry_port)
	{
		telemetry_port = in_telemetry_port;
	}

	void Model::finalize()
	{
		if (!root_workload)
			ROBOTICK_FATAL_EXIT("Model::validate: root workload must be set");

		// Validate any data-connections:
		for (size_t i = 0; i < data_connection_seeds.size(); ++i)
		{
			const DataConnectionSeed* conn = data_connection_seeds[i];
			const char* source = conn->source_field_path.c_str();
			const char* dest = conn->dest_field_path.c_str();

			// --- Validate source field path ---
			if (!strstr(source, ".outputs."))
			{
				ROBOTICK_FATAL_EXIT("Data connection error: source field path '%s' must use the 'outputs' structure.", source);
			}

			// Ensure valid source path format (at least 3 tokens: workload.outputs.field)
			if (count_char(source, '.') < 2)
			{
				ROBOTICK_FATAL_EXIT("Data connection error: malformed source field path '%s'. Expected format: workload.outputs.field", source);
			}

			// --- Validate destination field path ---
			if (!strstr(dest, ".inputs."))
			{
				ROBOTICK_FATAL_EXIT("Data connection error: destination field path '%s' must use the 'inputs' structure.", dest);
			}

			// Ensure valid destination path format (at least 3 tokens: workload.inputs.field)
			if (count_char(dest, '.') < 2)
			{
				ROBOTICK_FATAL_EXIT("Data connection error: malformed destination field path '%s'. Expected format: workload.inputs.field", dest);
			}

			// --- Check for duplicates ---
			for (size_t j = i + 1; j < data_connection_seeds.size(); ++j)
			{
				const char* dest_j = data_connection_seeds[j]->dest_field_path.c_str();
				if (string_equals(dest, dest_j))
				{
					ROBOTICK_FATAL_EXIT("Data connection error: destination field '%s' already has an incoming connection.", dest);
				}
			}
		}

		// Tick-rate validation
		for (const auto* parent_workload : workload_seeds)
		{
			const float parent_rate = parent_workload->tick_rate_hz;

			for (const auto* child_workload : parent_workload->children)
			{
				if (child_workload->tick_rate_hz > parent_rate)
				{
					ROBOTICK_FATAL_EXIT("Child workload '%s' has faster tick rate (%.2f Hz) than parent '%s' (%.2f Hz).",
						child_workload->unique_name.c_str(),
						child_workload->tick_rate_hz,
						parent_workload->unique_name.c_str(),
						parent_rate);
				}
			}
		}
	}

} // namespace robotick
