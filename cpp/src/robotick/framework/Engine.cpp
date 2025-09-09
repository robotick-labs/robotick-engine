// Copyright Robotick Labs
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"

#include "robotick/api.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/data/RemoteEngineConnection.h"
#include "robotick/framework/data/TelemetryServer.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/utils/TypeId.h"
#include "robotick/platform/PlatformEvents.h"
#include "robotick/platform/System.h"
#include "robotick/platform/Threading.h"
#include "robotick/platform/WebServer.h"

namespace robotick
{
	struct Engine::State
	{
		const Model* model = nullptr;
		bool is_running = false;

		WorkloadsBuffer workloads_buffer;

		TelemetryServer telemetry_server;

		const WorkloadInstanceInfo* root_instance = nullptr;
		HeapVector<WorkloadInstanceInfo> instances;
		Map<const char*, WorkloadInstanceInfo*> instances_by_unique_name;
		HeapVector<DataConnectionInfo> data_connections_all;
		HeapVector<DataConnectionInfo*> data_connections_acquired;

		HeapVector<RemoteEngineConnection> remote_engine_senders; // one sender per remote-engine we need to send data to
		RemoteEngineConnection remote_engines_receiver;			  // one receiver in case any engines need to send data to us
	};

	Engine::Engine()
		: state(new Engine::State())
	{
	}

	Engine::~Engine()
	{
		for (auto& instance : state->instances)
		{
			if (instance.workload_descriptor->destruct_fn)
			{
				void* instance_ptr = instance.get_ptr(*this);
				ROBOTICK_ASSERT(instance_ptr != nullptr);
				instance.workload_descriptor->destruct_fn(instance_ptr);
			}
		}
		delete state;
	}

	extern "C" void robotick_force_register_primitives();
	extern "C" void robotick_force_register_fixed_vector_types();
	extern "C" void robotick_force_register_vec3_types();

	void Engine::load(const Model& model)
	{
		// initialize the host-system
		System::initialize();

		// ensure out standard types don't get pruned by the linker:
		robotick_force_register_primitives();
		robotick_force_register_fixed_vector_types();
		robotick_force_register_vec3_types();

		if (!model.get_root_workload())
			ROBOTICK_FATAL_EXIT("Model has no root workload");

		if (state->model != nullptr)
			ROBOTICK_FATAL_EXIT("Engine has already been loaded, and cannot be reused");

		state->model = &model;

		// compute how big we need our workloads-buffer to be:
		const auto& seeds = model.get_workload_seeds();
		size_t workloads_cursor = 0;
		std::vector<size_t> offsets;
		for (const WorkloadSeed* seed : seeds)
		{
			const auto* workload_type = TypeRegistry::get().find_by_id(seed->type_id);
			if (!workload_type)
				ROBOTICK_FATAL_EXIT("Unknown workload type: %s", seed->type_id.get_debug_name());

			const size_t align = std::max<size_t>(workload_type->alignment, alignof(std::max_align_t));
			workloads_cursor = (workloads_cursor + align - 1) & ~(align - 1);
			offsets.push_back(workloads_cursor);
			workloads_cursor += workload_type->size;
		}

		// create our workloads-buffer, workload-instances info, and construct each workload:
		const size_t total_size = workloads_cursor;
		state->workloads_buffer = WorkloadsBuffer(total_size + DEFAULT_MAX_BLACKBOARDS_BYTES);
		uint8_t* buffer_ptr = state->workloads_buffer.raw_ptr();
		state->instances.initialize(seeds.size());

		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto* seed = seeds[i];
			const auto* workload_type = TypeRegistry::get().find_by_id(seed->type_id);

			const auto* workload_desc = workload_type->get_workload_desc();
			ROBOTICK_ASSERT(workload_desc != nullptr);

			uint8_t* ptr = buffer_ptr + offsets[i];

			WorkloadInstanceInfo& workload_instance_info = state->instances[i];
			workload_instance_info.offset_in_workloads_buffer = offsets[i];
			workload_instance_info.type = workload_type;
			workload_instance_info.workload_descriptor = workload_desc;
			workload_instance_info.seed = seed;

			// add it to our map for quick lookup by name
			state->instances_by_unique_name.insert(seed->unique_name.c_str(), &workload_instance_info);

			if (workload_desc->construct_fn)
				workload_desc->construct_fn(ptr);
		}

		// handle pre-load for each workload (we can multithread this in future, where platforms allow)
		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto& seed = seeds[i];
			const auto* workload_desc = state->instances[i].workload_descriptor;
			uint8_t* ptr = buffer_ptr + state->instances[i].offset_in_workloads_buffer;

			if (workload_desc->set_engine_fn)
				workload_desc->set_engine_fn(ptr, *this);

			if (seed->config.size() > 0 && workload_desc->config_desc)
			{
				ROBOTICK_ASSERT(workload_desc->config_offset != OFFSET_UNBOUND);
				DataConnectionUtils::apply_struct_field_values(ptr + workload_desc->config_offset, *workload_desc->config_desc, seed->config);
			}

			if (seed->inputs.size() > 0 && workload_desc->inputs_desc)
			{
				ROBOTICK_ASSERT(workload_desc->inputs_offset != OFFSET_UNBOUND);
				DataConnectionUtils::apply_struct_field_values(ptr + workload_desc->inputs_offset, *workload_desc->inputs_desc, seed->inputs);
			}

			if (workload_desc->pre_load_fn)
				workload_desc->pre_load_fn(ptr);
		}

		// compute our blackboard memory requirements, and bind our blackboards to that memory (they will store buffer-offsets relative to each
		// Blackboard header):
		size_t blackboard_size = compute_blackboard_memory_requirements(state->instances);
		if (blackboard_size > DEFAULT_MAX_BLACKBOARDS_BYTES)
			ROBOTICK_FATAL_EXIT("Blackboard memory (%zu) exceeds max allowed (%zu)", blackboard_size, DEFAULT_MAX_BLACKBOARDS_BYTES);

		if (blackboard_size > 0)
		{
			size_t start = workloads_cursor;
			workloads_cursor += blackboard_size;
			bind_blackboards_for_instances(state->instances, start);
		}

		// handle load for each workload (we can multithread this in future, where platforms allow)
		for (size_t i = 0; i < state->instances.size(); ++i)
		{
			auto& inst = state->instances[i];
			void* ptr = inst.get_ptr(*this);
			const auto* workload_desc = state->instances[i].workload_descriptor;

			if (workload_desc->load_fn)
				workload_desc->load_fn(ptr);
		}

		// hook-up children for each instance:
		for (size_t i = 0; i < seeds.size(); ++i)
		{
			auto& inst = state->instances[i];
			const auto* seed = seeds[i];

			ROBOTICK_ASSERT(inst.seed == seed);

			inst.children.initialize(seed->children.size());

			size_t child_index = 0;
			for (const auto* child_seed : seed->children)
			{
				ROBOTICK_ASSERT(child_seed != nullptr);

				const WorkloadInstanceInfo* child_inst = find_instance_info(child_seed->unique_name.c_str());
				ROBOTICK_ASSERT_MSG(child_inst != nullptr,
					"Child workload-instance named '%s' not found for workload-instance '%s'",
					child_seed->unique_name.c_str(),
					seed->unique_name.c_str());

				inst.children[child_index] = child_inst;
				child_index++;
			}
		}

		// create all data-connections:
		DataConnectionUtils::create(
			state->data_connections_all, state->workloads_buffer, model.get_data_connection_seeds(), state->instances_by_unique_name);

		const WorkloadInstanceInfo* root_instance = find_instance_info(model.get_root_workload()->unique_name.c_str());
		ROBOTICK_ASSERT(root_instance != nullptr);

		// call set_children_fn - allowing each child to take ownership (responsibility for propagating) each connection
		{
			if (root_instance->workload_descriptor->set_children_fn)
			{
				uint8_t* root_ptr = root_instance->get_ptr(*this);
				root_instance->workload_descriptor->set_children_fn(root_ptr, root_instance->children, state->data_connections_all);
			}
		}

		// allow Engine to acquire data-connections not handled by groups within the model:
		{
			// count how many data-connections we need to acquire:
			size_t num_to_acquire = 0;
			for (DataConnectionInfo& conn : state->data_connections_all)
			{
				if (conn.expected_handler == DataConnectionInfo::ExpectedHandler::DelegateToParent)
				{
					num_to_acquire++;
				}
				else if (conn.expected_handler == DataConnectionInfo::ExpectedHandler::Unassigned)
				{
					ROBOTICK_FATAL_EXIT("Unclaimed connection: %s -> %s", conn.seed->source_field_path.c_str(), conn.seed->dest_field_path.c_str());
				}
			}

			// allocate storage for data_connections_acquired
			state->data_connections_acquired.initialize(num_to_acquire);

			// acquire data_connections_acquired that Engine needs to propagate its self
			size_t acquired_index = 0;
			for (DataConnectionInfo& conn : state->data_connections_all)
			{
				if (conn.expected_handler == DataConnectionInfo::ExpectedHandler::DelegateToParent)
				{
					conn.expected_handler = DataConnectionInfo::ExpectedHandler::Engine;
					state->data_connections_acquired[acquired_index] = &conn;
					acquired_index++;
				}
			}
		}

		// call setup() on each instance that has that function
		for (auto& inst : state->instances)
		{
			if (inst.workload_descriptor->setup_fn)
				inst.workload_descriptor->setup_fn(inst.get_ptr(*this));
		}

		setup_remote_engine_senders(model);
		setup_remote_engines_receiver();

		state->root_instance = root_instance;
	}

	void Engine::run(const AtomicFlag& stop_after_next_tick_flag)
	{
		if (!state->root_instance)
			ROBOTICK_FATAL_EXIT("Root workload instance-info not set");

		const auto& root_info = *(state->root_instance);
		void* root_ptr = root_info.get_ptr(*this);

		if (!root_ptr)
			ROBOTICK_FATAL_EXIT("Root workload must have valid object-pointer - check it has been correctly registered");

		const float root_tick_rate_hz = root_info.seed->tick_rate_hz;
		if (root_tick_rate_hz <= 0.0)
			ROBOTICK_FATAL_EXIT("Root workload must have valid tick_rate_hz>0.0 - check your model settings");

		const auto root_tick_fn = root_info.workload_descriptor->tick_fn;
		if (root_tick_fn == nullptr)
			ROBOTICK_FATAL_EXIT("Root workload must have valid tick_fn - check it has been correctly registered");

		// Start all workloads
		for (auto& inst : state->instances)
		{
			if (inst.workload_descriptor->start_fn)
				inst.workload_descriptor->start_fn(inst.get_ptr(*this), inst.seed->tick_rate_hz);
		}

		state->telemetry_server.start(*this, state->model->get_telemetry_port());

		state->is_running = true;

		const auto child_tick_interval_sec = std::chrono::duration<float>(1.0f / root_tick_rate_hz);
		const auto child_tick_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(child_tick_interval_sec);

		const auto engine_start_time = std::chrono::steady_clock::now() - child_tick_interval;
		// ^- subtract tick-interval so initial delta is from tick-interval

		auto last_tick_time = engine_start_time;
		auto next_tick_time = engine_start_time;

		TickInfo tick_info;
		tick_info.workload_stats = &root_info.mutable_stats;

		do
		{
			const auto now = std::chrono::steady_clock::now();
			const auto ns_since_start = std::chrono::duration_cast<std::chrono::nanoseconds>(now - engine_start_time).count();
			const auto ns_since_last = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_tick_time).count();

			static const float s_1_nanosecond_sec = 1e-9F;

			tick_info.tick_count += 1;
			tick_info.time_now_ns = ns_since_start;
			tick_info.time_now = ns_since_start * s_1_nanosecond_sec;
			tick_info.delta_time = ns_since_last * s_1_nanosecond_sec;

			last_tick_time = now;

			// update remote data-connections
			tick_remote_engine_connections(tick_info);

			// update local data-connections
			for (const DataConnectionInfo* data_connection : state->data_connections_acquired)
			{
				data_connection->do_data_copy();
			}

			std::atomic_thread_fence(std::memory_order_release);

			root_tick_fn(root_ptr, tick_info);

			const auto now_post = std::chrono::steady_clock::now();
			root_info.mutable_stats.last_tick_duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now_post - now).count();
			root_info.mutable_stats.last_time_delta_ns = ns_since_last;

			next_tick_time += child_tick_interval;
			Thread::hybrid_sleep_until(next_tick_time);

		} while (!stop_after_next_tick_flag.is_set() && !should_exit_application());

		state->is_running = false;

		for (auto& inst : state->instances)
		{
			if (inst.workload_descriptor->stop_fn)
				inst.workload_descriptor->stop_fn(inst.get_ptr(*this));
		}
	}

	void Engine::setup_remote_engines_receiver()
	{
		ROBOTICK_INFO("Setting up remote engines receiver...");

		const char* my_model_name = state->model->get_model_name();
		state->remote_engines_receiver.configure_receiver(my_model_name);

		state->remote_engines_receiver.set_field_binder(
			[&](const char* path, RemoteEngineConnection::Field& out)
			{
				auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*this, path);
				if (ptr == nullptr)
				{
					ROBOTICK_FATAL_EXIT("Engine::setup_remote_engines_receiver() - unable to resolve field path: %s", path);
				}

				out.path = path;
				out.recv_ptr = ptr;
				out.size = size;
				out.type_desc = field_desc->find_type_descriptor();
				ROBOTICK_ASSERT(out.type_desc != nullptr);

				return true;
			});
	}

	void Engine::setup_remote_engine_senders(const Model& model)
	{
		const auto& remote_model_seeds = model.get_remote_models();
		if (remote_model_seeds.size() == 0)
		{
			return;
		}

		ROBOTICK_INFO("Setting up remote engine senders...");

		state->remote_engine_senders.initialize(remote_model_seeds.size());

		uint32_t remote_engine_index = 0;

		for (const RemoteModelSeed* remote_model_seed : remote_model_seeds)
		{
			ROBOTICK_ASSERT(remote_model_seed != nullptr);

			if (remote_model_seed->remote_data_connection_seeds.size() == 0)
			{
				ROBOTICK_WARNING("Remote model '%s' has no remote data-connections - skipping adding remote_engine_sender for it",
					remote_model_seed->model_name.c_str());

				continue;
			}

			RemoteEngineConnection& remote_engine_connection = state->remote_engine_senders[remote_engine_index];
			remote_engine_index++;

			const char* my_model_name = state->model->get_model_name();
			remote_engine_connection.configure_sender(my_model_name, remote_model_seed->model_name.c_str());

			for (const auto* remote_data_connection_seed : remote_model_seed->remote_data_connection_seeds)
			{
				const char* source_field_path = remote_data_connection_seed->source_field_path.c_str();

				auto [ptr, size, field_desc] = DataConnectionUtils::find_field_info(*this, source_field_path);
				if (ptr == nullptr)
				{
					ROBOTICK_FATAL_EXIT("Engine::setup_remote_engine_senders() - unable to resolve source field path: %s", source_field_path);
				}

				RemoteEngineConnection::Field remote_field;
				remote_field.path = remote_data_connection_seed->dest_field_path.c_str();
				remote_field.send_ptr = ptr;
				remote_field.size = size;
				remote_field.type_desc = field_desc->find_type_descriptor();
				ROBOTICK_ASSERT(remote_field.type_desc != nullptr);

				remote_engine_connection.register_field(remote_field);
			}
		}
	}

	bool Engine::is_running() const
	{
		return state && state->is_running;
	}

	const WorkloadInstanceInfo* Engine::get_root_instance_info() const
	{
		return state->root_instance;
	}

	const WorkloadInstanceInfo* Engine::find_instance_info(const char* unique_name) const
	{
		WorkloadInstanceInfo** found_instance_info = state->instances_by_unique_name.find(unique_name);
		return found_instance_info ? *found_instance_info : nullptr;
	}

	void* Engine::find_instance(const char* unique_name) const
	{
		const WorkloadInstanceInfo* found_instance_info = find_instance_info(unique_name);
		if (found_instance_info)
		{
			return found_instance_info->get_ptr(*this);
		}

		return nullptr;
	}

	const HeapVector<WorkloadInstanceInfo>& Engine::get_all_instance_info() const
	{
		return state->instances;
	}

	const Map<const char*, WorkloadInstanceInfo*>& Engine::get_all_instance_info_map() const
	{
		return state->instances_by_unique_name;
	}

	const HeapVector<DataConnectionInfo>& Engine::get_all_data_connections() const
	{
		return state->data_connections_all;
	}

	WorkloadsBuffer& Engine::get_workloads_buffer() const
	{
		return state->workloads_buffer;
	}

	size_t Engine::compute_blackboard_memory_requirements(const HeapVector<WorkloadInstanceInfo>& instances)
	{
		size_t total = 0;
		for (const auto& instance : instances)
		{
			const WorkloadDescriptor* workload_descriptor = instance.workload_descriptor;
			if (!workload_descriptor)
				continue;

			void* instance_ptr = instance.get_ptr(*this);
			ROBOTICK_ASSERT(instance_ptr != nullptr);

			auto accumulate = [&](const TypeDescriptor* struct_type_desc, const size_t struct_offset)
			{
				if (!struct_type_desc)
					return;

				const StructDescriptor* struct_desc = struct_type_desc->get_struct_desc();
				if (!struct_desc)
				{
					ROBOTICK_FATAL_EXIT("Workload '%s' has invalid struct descriptor of type '%s'",
						instance.seed->unique_name.c_str(),
						struct_type_desc->name.c_str());
				}

				for (const FieldDescriptor& field : struct_desc->fields)
				{
					if (field.type_id == GET_TYPE_ID(Blackboard))
					{
						ROBOTICK_ASSERT(field.offset_within_container != OFFSET_UNBOUND);

						const Blackboard& blackboard =
							field.get_data<Blackboard>(state->workloads_buffer, instance, *struct_type_desc, struct_offset);
						total += blackboard.get_info().total_datablock_size;
					}
				}
			};

			accumulate(workload_descriptor->config_desc, workload_descriptor->config_offset);
			accumulate(workload_descriptor->inputs_desc, workload_descriptor->inputs_offset);
			accumulate(workload_descriptor->outputs_desc, workload_descriptor->outputs_offset);
		}
		return total;
	}

	void Engine::bind_blackboards_in_struct(
		WorkloadInstanceInfo& instance, const TypeDescriptor& struct_type_desc, const size_t struct_offset, size_t& blackboard_storage_offset)
	{
		const StructDescriptor* struct_desc = struct_type_desc.get_struct_desc();
		ROBOTICK_ASSERT(struct_desc != nullptr);

		for (const FieldDescriptor& field : struct_desc->fields)
		{
			if (field.type_id == GET_TYPE_ID(Blackboard))
			{
				Blackboard& blackboard = field.get_data<Blackboard>(state->workloads_buffer, instance, struct_type_desc, struct_offset);
				blackboard.bind(blackboard_storage_offset);
			}
		}
	}

	void Engine::bind_blackboards_for_instances(HeapVector<WorkloadInstanceInfo>& instances, size_t start_offset)
	{
		for (auto& instance : instances)
		{
			const WorkloadDescriptor* workload_desc = instance.workload_descriptor;
			if (!workload_desc)
				continue;

			if (workload_desc->config_desc)
				bind_blackboards_in_struct(instance, *workload_desc->config_desc, workload_desc->config_offset, start_offset);
			if (workload_desc->inputs_desc)
				bind_blackboards_in_struct(instance, *workload_desc->inputs_desc, workload_desc->inputs_offset, start_offset);
			if (workload_desc->outputs_desc)
				bind_blackboards_in_struct(instance, *workload_desc->outputs_desc, workload_desc->outputs_offset, start_offset);
		}
	}

	void Engine::tick_remote_engine_connections(const TickInfo& tick_info)
	{
		const bool enable_remote_engines_receiver = (state->remote_engine_senders.size() == 0); // placeholder for having config setting

		if (enable_remote_engines_receiver)
		{
			state->remote_engines_receiver.tick(tick_info);
		}

		for (auto& remote_engine_sender : state->remote_engine_senders)
		{
			remote_engine_sender.tick(tick_info);
		}
	}

} // namespace robotick
