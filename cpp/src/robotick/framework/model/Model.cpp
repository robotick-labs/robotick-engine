// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/model/Model.h"

#include "robotick/api_base.h"
#include "robotick/framework/model/DataConnectionSeed.h"
#include "robotick/framework/model/RemoteModelSeed.h"
#include "robotick/framework/model/WorkloadSeed.h"

namespace robotick
{

#ifdef ROBOTICK_ENABLE_MODEL_HEAP

	WorkloadSeed& Model::add()
	{
		if (root_workload)
		{
			ROBOTICK_FATAL_EXIT("Cannot add workloads after root has been set. Root must be set last.");
		}

		if (workload_seeds.size() > 0)
		{
			ROBOTICK_FATAL_EXIT("Model::add() (dynamic models) called after Model::use_workload_seeds() (non-dynamic models) has been called");
		}

		WorkloadSeed& seed = workload_seeds_storage.push_back();
		return seed;
	}

	WorkloadSeed& Model::add(const char* type_name, const char* name)
	{
		return add().set_type_name(type_name).set_name(name);
	}

	void Model::connect(const char* source_field_path, const char* dest_field_path)
	{
		if (root_workload)
			ROBOTICK_FATAL_EXIT("Cannot add connections after root has been set. Root must be set last.");

		if (!source_field_path || !dest_field_path || !*source_field_path || !*dest_field_path)
			ROBOTICK_FATAL_EXIT("Field paths must be non-empty");

		if (strcmp(source_field_path, dest_field_path) == 0)
			ROBOTICK_FATAL_EXIT("Source and destination field paths are identical: %s", dest_field_path);

		if (source_field_path[0] == '|')
			ROBOTICK_FATAL_EXIT("Source field paths cannot be remote: %s", source_field_path);

		if (dest_field_path[0] == '|')
		{
			connect_remote(source_field_path, dest_field_path);
			return;
		}

		for (const DataConnectionSeed& existing : data_connection_seeds_storage)
		{
			if (strcmp(existing.dest_field_path.c_str(), dest_field_path) == 0)
			{
				ROBOTICK_FATAL_EXIT("Destination field already has an incoming connection: %s", dest_field_path);
			}
		}

		DataConnectionSeed& data_connection_seed = data_connection_seeds_storage.push_back();
		data_connection_seed.set_source_field_path(source_field_path);
		data_connection_seed.set_dest_field_path(dest_field_path);
	}

	void Model::connect_remote(const char* source_field_path, const char* dest_field_path)
	{
		const char* pipe1 = strchr(dest_field_path + 1, '|');
		if (!pipe1)
			ROBOTICK_FATAL_EXIT("Invalid remote field format: %s", dest_field_path);

		FixedString64 model_id(dest_field_path + 1, pipe1 - dest_field_path - 1);
		const char* remote_field_path = pipe1 + 1;

		RemoteModelSeed* found_remote_model = nullptr;
		for (RemoteModelSeed& rm : remote_models_storage)
		{
			if (rm.model_name == model_id)
			{
				found_remote_model = &rm;
				break;
			}
		}

		if (!found_remote_model)
			ROBOTICK_FATAL_EXIT("Unknown remote model ID: %s", model_id.c_str());

		if (!found_remote_model->model)
			ROBOTICK_FATAL_EXIT("No model set on remote model ID: %s", model_id.c_str());

		for (const auto& s : found_remote_model->remote_data_connection_seeds_storage)
		{
			if (s.dest_field_path == remote_field_path)
				ROBOTICK_FATAL_EXIT(
					"Remote destination field already has an incoming remote-connection: |%s|%s", model_id.c_str(), remote_field_path);
		}

		for (const auto& s : found_remote_model->model->get_data_connection_seeds())
		{
			if (strcmp(s->dest_field_path.c_str(), remote_field_path) == 0)
				ROBOTICK_FATAL_EXIT("Remote destination field already has an incoming local-connection: |%s|%s", model_id.c_str(), remote_field_path);
		}

		DataConnectionSeed& data_connection_seed = found_remote_model->remote_data_connection_seeds_storage.push_back();
		data_connection_seed.set_source_field_path(source_field_path);
		data_connection_seed.set_dest_field_path(remote_field_path);
	}

	void Model::add_remote_model(const Model& remote_model, const char* model_name, const char* comms_channel)
	{
		if (!remote_model.root_workload)
			ROBOTICK_FATAL_EXIT("add_remote_model: remote_model must have root_workload and been finalised");

		if (!model_name || !*model_name)
			ROBOTICK_FATAL_EXIT("add_remote_model: model_name must not be empty");

		if (!comms_channel || !*comms_channel)
			ROBOTICK_FATAL_EXIT("add_remote_model: comms_channel must not be empty");

		if (remote_model.get_workload_seeds().size() == 0)
			ROBOTICK_FATAL_EXIT("add_remote_model: remote_model must contain at least one workload");

		for (const auto& rm : remote_models_storage)
		{
			if (strcmp(rm.model_name.c_str(), model_name) == 0)
				ROBOTICK_FATAL_EXIT("add_remote_model: a remote model with name '%s' already exists", model_name);
		}

		const char* sep = strchr(comms_channel, ':');
		if (!sep)
			ROBOTICK_FATAL_EXIT("add_remote_model: invalid comms_channel format, expected <mode>:<address>");

		FixedString64 mode(comms_channel, sep - comms_channel);
		const char* address = sep + 1;

		RemoteModelSeed& seed = remote_models_storage.push_back();
		seed.set_model_name(model_name);
		seed.set_comms_channel(address);

		seed.model = &remote_model;

		if (mode == "ip")
		{
			seed.comms_mode = RemoteModelSeed::Mode::IP;
		}
		else
		{
			ROBOTICK_FATAL_EXIT("add_remote_model: unsupported comms_channel mode: '%s'", mode.c_str());
		}
	}
	void Model::bake_dynamic_workloads()
	{
		baked_workload_ptrs.initialize(workload_seeds_storage.size());

		size_t index = 0;
		for (const WorkloadSeed& seed : workload_seeds_storage)
		{
			baked_workload_ptrs[index] = &seed;
			index++;
		}

		workload_seeds = ArrayView<const WorkloadSeed*>(baked_workload_ptrs.data(), baked_workload_ptrs.size());
	}

	void Model::bake_dynamic_data_connections()
	{
		baked_data_connection_ptrs.initialize(data_connection_seeds_storage.size());

		size_t index = 0;
		for (const DataConnectionSeed& seed : data_connection_seeds_storage)
		{
			baked_data_connection_ptrs[index] = &seed;
			index++;
		}

		data_connection_seeds = ArrayView<const DataConnectionSeed*>(baked_data_connection_ptrs.data(), baked_data_connection_ptrs.size());
	}

	void Model::bake_dynamic_remote_models()
	{
		baked_remote_model_ptrs.initialize(remote_models_storage.size());

		size_t index = 0;
		for (RemoteModelSeed& seed : remote_models_storage)
		{
			baked_remote_model_ptrs[index] = &seed;
			index++;

			seed.bake_dynamic_remote_connections();
		}

		remote_models = ArrayView<const RemoteModelSeed*>(baked_remote_model_ptrs.data(), baked_remote_model_ptrs.size());
	}

#endif // ROBOTICK_ENABLE_MODEL_HEAP

	void Model::use_workload_seeds(const WorkloadSeed** all_workloads, size_t num_workloads)
	{
		if (workload_seeds_storage.size() > 0)
		{
			ROBOTICK_FATAL_EXIT("Model::use_workload_seeds() (non-dynamic models) called after Model::add() (dynamic models) has been called");
		}

		workload_seeds = ArrayView<const WorkloadSeed*>(all_workloads, num_workloads);
	}

	void Model::use_data_connection_seeds(const DataConnectionSeed** in_connections, size_t num_connections)
	{
		if (data_connection_seeds_storage.size() > 0)
		{
			ROBOTICK_FATAL_EXIT(
				"Model::use_data_connection_seeds() (non-dynamic models) called after Model::connect() (dynamic models) has been called");
		}

		data_connection_seeds = ArrayView<const DataConnectionSeed*>(in_connections, num_connections);
	}

	void Model::set_root_workload(const WorkloadSeed& root, bool auto_finalize)
	{
		root_workload = &root;

		if (auto_finalize)
		{
			finalize();
		}
	}

	void Model::finalize()
	{
		if (!root_workload)
			ROBOTICK_FATAL_EXIT("Model::validate: root workload must be set");

#ifdef ROBOTICK_ENABLE_MODEL_HEAP
		const bool has_dynamic_workloads = workload_seeds_storage.size() > 0;
		if (has_dynamic_workloads)
		{
			bake_dynamic_workloads();
			bake_dynamic_data_connections();
			bake_dynamic_remote_models();
		}
#endif

		// Ensure unique input destinations
		for (size_t i = 0; i < data_connection_seeds.size(); ++i)
		{
			const char* dest_i = data_connection_seeds[i]->dest_field_path.c_str();

			for (size_t j = i + 1; j < data_connection_seeds.size(); ++j)
			{
				const char* dest_j = data_connection_seeds[j]->dest_field_path.c_str();
				if (strcmp(dest_i, dest_j) == 0)
				{
					ROBOTICK_FATAL_EXIT("Data connection error: destination field '%s' already has an incoming connection.", dest_i);
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
						child_workload->name.c_str(),
						child_workload->tick_rate_hz,
						parent_workload->name.c_str(),
						parent_rate);
				}
			}
		}
	}

} // namespace robotick