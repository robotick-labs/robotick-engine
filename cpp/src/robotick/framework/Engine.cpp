// Copyright Robotick contributors
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"

#include "robotick/api.h"
#include "robotick/framework/concurrency/Atomic.h"
#include "robotick/framework/concurrency/Thread.h"
#include "robotick/framework/containers/List.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/DataConnection.h"
#include "robotick/framework/data/RemoteEngineConnections.h"
#include "robotick/framework/data/TelemetryServer.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/model/Model.h"
#include "robotick/framework/services/WebServer.h"
#include "robotick/framework/system/PlatformEvents.h"
#include "robotick/framework/system/System.h"
#include "robotick/framework/time/Clock.h"
#include "robotick/framework/utils/TypeId.h"

#include <cstddef>
#include <cstdint>

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
		List<DataConnectionInputHandle> data_connection_input_handles;
		Map<const char*, DataConnectionInputHandle*, 128> data_connection_input_handles_by_path;
		Map<uintptr_t, DataConnectionInputHandle*, 128> data_connection_input_handles_by_dest_ptr;
		Map<uint16_t, DataConnectionInputHandle*, 128> data_connection_input_handles_by_id;
		uint16_t next_data_connection_input_handle_id = 1;

		RemoteEngineConnections remote_engine_connections;
	};

	Engine::Engine()
		: state(new Engine::State())
	{
	}

	Engine::~Engine()
	{
		state->telemetry_server.stop();
		state->remote_engine_connections.stop();

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
	extern "C" void robotick_force_register_quat_types();
	extern "C" void robotick_force_register_pose_types();
	extern "C" void robotick_force_register_transform_types();

	// Prevent integer overflow while sizing contiguous chunks so layout stays deterministic even near SIZE_MAX.
	// The workloads buffer is preallocated once, so a bad descriptor must be rejected rather than silently wrapping.
	static bool safe_add_size(size_t lhs, size_t rhs, size_t& out)
	{
		const size_t max_size = SIZE_MAX;
		if (rhs > max_size - lhs)
			return false;
		out = lhs + rhs;
		return true;
	}

	// Clamp to max_align_t so the placement-new construction that follows always satisfies C++ object requirements.
	static size_t max_align_for_type(size_t alignment)
	{
		return (alignment > alignof(max_align_t)) ? alignment : alignof(max_align_t);
	}

	static bool align_workloads_cursor_with_alignment(size_t alignment, size_t& workloads_cursor)
	{
		if (alignment == 0)
			return false;
		size_t remainder = workloads_cursor % alignment;
		if (remainder != 0)
		{
			size_t delta = alignment - remainder;
			size_t aligned_cursor = 0;
			if (!safe_add_size(workloads_cursor, delta, aligned_cursor))
				return false;
			workloads_cursor = aligned_cursor;
		}
		return true;
	}

	// Ensure the cursor respects the max alignment of the upcoming type so every allocation remains well-aligned.
	static bool align_workloads_cursor_for_type(const TypeDescriptor& type, size_t& workloads_cursor)
	{
		const size_t alignment = max_align_for_type(type.alignment);
		// Move the cursor forward so that the next placement-new starts at an address compatible with the target type.
		return align_workloads_cursor_with_alignment(alignment, workloads_cursor);
	}

	// Reserve space for the named type and record the advanced cursor so the next workload starts immediately after it.
	static bool increment_workloads_cursor_for_type(const TypeDescriptor& type, size_t& workloads_cursor)
	{
		if (type.size == 0)
			return false;
		size_t next_cursor = 0;
		if (!safe_add_size(workloads_cursor, type.size, next_cursor))
			return false;
		// Advance past the object we just allocated so the following type begins immediately afterwards.
		workloads_cursor = next_cursor;
		return true;
	}

	static size_t compute_dynamic_struct_storage_region_end(
		size_t workloads_cursor, size_t dynamic_struct_storage_start_alignment, size_t dynamic_struct_storage_size, size_t& out_start_offset)
	{
		size_t dynamic_struct_storage_start = workloads_cursor;
		if (dynamic_struct_storage_size > 0 &&
			!align_workloads_cursor_with_alignment(dynamic_struct_storage_start_alignment, dynamic_struct_storage_start))
		{
			ROBOTICK_FATAL_EXIT("Workloads buffer alignment overflow while reserving dynamic struct storage");
		}

		size_t dynamic_struct_storage_end = dynamic_struct_storage_start;
		if (dynamic_struct_storage_size > 0 && !safe_add_size(dynamic_struct_storage_start, dynamic_struct_storage_size, dynamic_struct_storage_end))
		{
			ROBOTICK_FATAL_EXIT("Workloads buffer overflow while reserving dynamic struct storage (%zu + %zu)",
				dynamic_struct_storage_start,
				dynamic_struct_storage_size);
		}

		out_start_offset = dynamic_struct_storage_start;
		return dynamic_struct_storage_end;
	}

	void Engine::load(const Model& model)
	{
		ROBOTICK_INFO("Loading model: %s", model.get_model_name());

		// initialize the host-system
		System::initialize();

		// ensure our standard types don't get pruned by the linker, and then seal the registry:
		robotick_force_register_primitives();
		robotick_force_register_fixed_vector_types();
		robotick_force_register_vec3_types();
		robotick_force_register_quat_types();
		robotick_force_register_pose_types();
		robotick_force_register_transform_types();
		TypeRegistry::get().seal();

		if (!model.get_root_workload())
			ROBOTICK_FATAL_EXIT("Model has no root workload");

		if (state->model != nullptr)
			ROBOTICK_FATAL_EXIT("Engine has already been loaded, and cannot be reused");

		state->model = &model;

		const size_t inline_workloads_size = compute_inline_workloads_buffer_size();

		// Preflight the workload graph once to discover exact dynamic-struct storage requirements.
		state->workloads_buffer = WorkloadsBuffer(inline_workloads_size);
		construct_workload_instances();
		run_workload_pre_load_pass();

		size_t planned_dynamic_struct_storage_start_alignment = 1;
		const size_t planned_dynamic_struct_storage_size =
			compute_dynamic_struct_storage_requirements(state->instances, planned_dynamic_struct_storage_start_alignment);

		destroy_constructed_workload_instances();
		reset_loaded_workload_state();

		size_t planned_dynamic_struct_storage_start = inline_workloads_size;
		const size_t final_workloads_buffer_size = compute_dynamic_struct_storage_region_end(inline_workloads_size,
			planned_dynamic_struct_storage_start_alignment,
			planned_dynamic_struct_storage_size,
			planned_dynamic_struct_storage_start);

		// Rebuild for real into the final exact-sized WorkloadsBuffer, then validate the plan before binding.
		state->workloads_buffer = WorkloadsBuffer(final_workloads_buffer_size);
		construct_workload_instances();
		run_workload_pre_load_pass();

		size_t final_dynamic_struct_storage_start_alignment = 1;
		const size_t final_dynamic_struct_storage_size =
			compute_dynamic_struct_storage_requirements(state->instances, final_dynamic_struct_storage_start_alignment);
		if (final_dynamic_struct_storage_start_alignment != planned_dynamic_struct_storage_start_alignment ||
			final_dynamic_struct_storage_size != planned_dynamic_struct_storage_size)
		{
			ROBOTICK_FATAL_EXIT(
				"Dynamic struct storage plan changed between preflight and final load (planned align=%zu size=%zu, final align=%zu size=%zu)",
				planned_dynamic_struct_storage_start_alignment,
				planned_dynamic_struct_storage_size,
				final_dynamic_struct_storage_start_alignment,
				final_dynamic_struct_storage_size);
		}

		size_t dynamic_struct_storage_start = inline_workloads_size;
		size_t workloads_cursor = compute_dynamic_struct_storage_region_end(
			inline_workloads_size, final_dynamic_struct_storage_start_alignment, final_dynamic_struct_storage_size, dynamic_struct_storage_start);

		if (final_dynamic_struct_storage_size > 0)
		{
			bind_dynamic_struct_storage_for_instances(state->instances, dynamic_struct_storage_start);
		}

		state->workloads_buffer.set_size_used(workloads_cursor);

		// post-dynamic-struct-storage config pass:
		const auto& seeds = model.get_workload_seeds();
		uint8_t* buffer_ptr = state->workloads_buffer.raw_ptr();
		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto& seed = seeds[i];
			const auto* workload_desc = state->instances[i].workload_descriptor;
			uint8_t* ptr = buffer_ptr + state->instances[i].offset_in_workloads_buffer;

			if (seed->config.size() > 0 && workload_desc->config_desc)
			{
				ROBOTICK_ASSERT(workload_desc->config_offset != OFFSET_UNBOUND);

				const bool fatalExitIfNotFound = true;
				DataConnectionUtils::apply_struct_field_values(
					ptr + workload_desc->config_offset, *workload_desc->config_desc, seed->config, fatalExitIfNotFound);
			}

			if (seed->inputs.size() > 0 && workload_desc->inputs_desc)
			{
				ROBOTICK_ASSERT(workload_desc->inputs_offset != OFFSET_UNBOUND);

				const bool fatalExitIfNotFound = true;
				DataConnectionUtils::apply_struct_field_values(
					ptr + workload_desc->inputs_offset, *workload_desc->inputs_desc, seed->inputs, fatalExitIfNotFound);
			}
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

		for (DataConnectionInfo& conn : state->data_connections_all)
		{
			conn.dest_handle = find_or_create_data_connection_input_handle(
				conn.seed ? conn.seed->dest_field_path.c_str() : nullptr, conn.dest_ptr, conn.size, conn.type);
		}

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

		ROBOTICK_ASSERT(state->model != nullptr);
		state->remote_engine_connections.setup(*this, *(state->model));
		state->telemetry_server.setup(*this);

		state->root_instance = root_instance;

		ROBOTICK_INFO("Loading complete for model: %s", model.get_model_name());
	}

	void Engine::run(const AtomicFlag& stop_after_next_tick_flag)
	{
		ROBOTICK_INFO("Running model: %s", state->model->get_model_name());

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

		// start_fn always runs on the same thread that will perform ticks so workloads can safely cache thread-affine handles.
		if (root_info.workload_descriptor->start_fn)
			root_info.workload_descriptor->start_fn(root_ptr, root_tick_rate_hz);

		state->telemetry_server.start(*this, state->model->get_telemetry_port());

		state->is_running = true;

		const auto child_tick_interval = Clock::from_seconds(1.0f / root_tick_rate_hz);
		const auto engine_start_time = Clock::now() - child_tick_interval;
		// ^- subtract tick-interval so initial delta is from tick-interval

		auto last_tick_time = engine_start_time;
		auto next_tick_time = engine_start_time;

		// TickInfo is reused every iteration; we update its running clock/delta fields so consumers never see partially
		// initialized values.
		TickInfo tick_info;
		tick_info.workload_stats = root_info.workload_stats;
		tick_info.tick_rate_hz = root_tick_rate_hz;

		do
		{
			const auto now = Clock::now();
			const auto ns_since_start = Clock::to_nanoseconds(now - engine_start_time).count();
			const auto ns_since_last = Clock::to_nanoseconds(now - last_tick_time).count();

			constexpr float s_1_nanosecond_sec = 1e-9F;

			tick_info.tick_count += 1;
			tick_info.time_now_ns = ns_since_start;
			tick_info.time_now = ns_since_start * s_1_nanosecond_sec;
			tick_info.delta_time = ns_since_last * s_1_nanosecond_sec;

			last_tick_time = now;

			// Open the seqlock window before mutating workload memory so telemetry readers can detect "write in progress" (odd seq).
			state->workloads_buffer.mark_frame_write_begin();

			// update remote data-connections
			state->remote_engine_connections.tick(tick_info);

			// update local data-connections
			for (const DataConnectionInfo* data_connection : state->data_connections_acquired)
			{
				data_connection->do_data_copy();
			}

			// Apply pending telemetry-originated input writes after connection propagation and before tick.
			state->telemetry_server.apply_pending_input_writes();

			// Ensure all published data writes are visible before workloads read them (cross-thread barrier via Atomic helpers)
			thread_fence_release();

			root_tick_fn(root_ptr, tick_info);

			const auto now_post = Clock::now();
			const uint32_t duration_ns = detail::clamp_to_uint32(Clock::to_nanoseconds(now_post - now).count());
			const uint64_t budget_ns_raw = Clock::to_nanoseconds(child_tick_interval).count();
			const uint32_t budget_ns = detail::clamp_to_uint32(budget_ns_raw);

			const uint32_t clamped_delta_ns = detail::clamp_to_uint32(ns_since_last);

			// Update the per-workload stats in-place so telemetry can report overruns without introducing dynamic allocations.
			root_info.workload_stats->record_tick_sample(duration_ns, clamped_delta_ns, budget_ns);
			root_info.workload_stats->tick_count++;

			// Close the seqlock window after all tick writes so telemetry readers can treat this frame as stable (even seq).
			state->workloads_buffer.mark_frame_write_end();

			next_tick_time += child_tick_interval;
			Thread::hybrid_sleep_until(next_tick_time);

		} while (!stop_after_next_tick_flag.is_set() && !should_exit_application());

		ROBOTICK_INFO("Engine stopping for model: %s", state->model->get_model_name());

		state->is_running = false;

		for (auto& inst : state->instances)
		{
			if (inst.workload_descriptor->stop_fn)
				inst.workload_descriptor->stop_fn(inst.get_ptr(*this));
		}

		state->remote_engine_connections.stop();
		state->telemetry_server.stop();

		ROBOTICK_INFO("Engine stopped for model: %s", state->model->get_model_name());
	}

	bool Engine::is_running() const
	{
		return state->is_running;
	}

	const char* Engine::get_model_name() const
	{
		return (state != nullptr && state->model != nullptr) ? state->model->get_model_name() : "model_not_set";
	}

	const WorkloadInstanceInfo* Engine::get_root_instance_info() const
	{
		return state->root_instance;
	}

	const Model& Engine::get_model() const
	{
		ROBOTICK_ASSERT(state != nullptr);
		ROBOTICK_ASSERT(state->model != nullptr);
		return *state->model;
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

	DataConnectionInputHandle* Engine::find_data_connection_input_handle_by_path(const char* path) const
	{
		if (!path)
		{
			return nullptr;
		}
		DataConnectionInputHandle** found = state->data_connection_input_handles_by_path.find(path);
		return found ? *found : nullptr;
	}

	DataConnectionInputHandle* Engine::find_data_connection_input_handle_by_dest_ptr(const void* dest_ptr) const
	{
		if (!dest_ptr)
		{
			return nullptr;
		}
		DataConnectionInputHandle** found = state->data_connection_input_handles_by_dest_ptr.find(reinterpret_cast<uintptr_t>(dest_ptr));
		return found ? *found : nullptr;
	}

	DataConnectionInputHandle* Engine::find_data_connection_input_handle_by_handle_id(uint16_t handle_id) const
	{
		if (handle_id == 0)
		{
			return nullptr;
		}
		DataConnectionInputHandle** found = state->data_connection_input_handles_by_id.find(handle_id);
		return found ? *found : nullptr;
	}

	DataConnectionInputHandle* Engine::find_data_connection_input_handle_by_overlap(const void* dest_ptr, size_t size) const
	{
		if (!dest_ptr || size == 0)
		{
			return nullptr;
		}

		const uintptr_t target_begin = reinterpret_cast<uintptr_t>(dest_ptr);
		const uintptr_t target_end = target_begin + size;
		for (const DataConnectionInputHandle& handle : state->data_connection_input_handles)
		{
			if (!handle.dest_ptr || handle.size == 0)
			{
				continue;
			}

			const uintptr_t handle_begin = reinterpret_cast<uintptr_t>(handle.dest_ptr);
			const uintptr_t handle_end = handle_begin + handle.size;
			if (target_begin < handle_end && handle_begin < target_end)
			{
				return const_cast<DataConnectionInputHandle*>(&handle);
			}
		}

		return nullptr;
	}

	DataConnectionInputHandle* Engine::find_or_create_data_connection_input_handle(const char* path, void* dest_ptr, size_t size, TypeId type)
	{
		if (!path || !dest_ptr || size == 0)
		{
			return nullptr;
		}

		if (DataConnectionInputHandle* existing_by_path = find_data_connection_input_handle_by_path(path))
		{
			ROBOTICK_ASSERT(existing_by_path->dest_ptr == dest_ptr);
			ROBOTICK_ASSERT(existing_by_path->size == size);
			ROBOTICK_ASSERT(existing_by_path->type == type);
			return existing_by_path;
		}

		if (DataConnectionInputHandle* existing_by_ptr = find_data_connection_input_handle_by_dest_ptr(dest_ptr))
		{
			ROBOTICK_ASSERT(existing_by_ptr->size == size);
			ROBOTICK_ASSERT(existing_by_ptr->type == type);
			if (existing_by_ptr->path.empty())
			{
				existing_by_ptr->path = path;
				state->data_connection_input_handles_by_path.insert(existing_by_ptr->path.c_str(), existing_by_ptr);
			}
			return existing_by_ptr;
		}

		if (state->next_data_connection_input_handle_id == 0)
		{
			ROBOTICK_FATAL_EXIT("DataConnectionInputHandle id overflow");
		}

		DataConnectionInputHandle& created = state->data_connection_input_handles.push_back();
		created.handle_id = state->next_data_connection_input_handle_id++;
		created.path = path;
		created.dest_ptr = dest_ptr;
		created.size = size;
		created.type = type;
		created.set_enabled(true);

		state->data_connection_input_handles_by_path.insert(created.path.c_str(), &created);
		state->data_connection_input_handles_by_dest_ptr.insert(reinterpret_cast<uintptr_t>(created.dest_ptr), &created);
		state->data_connection_input_handles_by_id.insert(created.handle_id, &created);
		return &created;
	}

	WorkloadsBuffer& Engine::get_workloads_buffer() const
	{
		return state->workloads_buffer;
	}

	TelemetryServer& Engine::get_telemetry_server() const
	{
		return state->telemetry_server;
	}

	size_t Engine::compute_inline_workloads_buffer_size() const
	{
		ROBOTICK_ASSERT(state != nullptr);
		ROBOTICK_ASSERT(state->model != nullptr);

		const auto* workload_stats_type = TypeRegistry::get().find_by_id(GET_TYPE_ID(WorkloadInstanceStats));
		ROBOTICK_ASSERT_MSG(workload_stats_type, "Type 'WorkloadInstanceStats' not registered - this should never happen");

		const auto& seeds = state->model->get_workload_seeds();
		size_t total_size = 0;
		for (const WorkloadSeed* seed : seeds)
		{
			const auto* workload_type = TypeRegistry::get().find_by_id(seed->type_id);
			ROBOTICK_ASSERT_MSG(workload_type, "Unknown workload type: %s", seed->type_id.get_debug_name());

			if (!align_workloads_cursor_for_type(*workload_stats_type, total_size))
				ROBOTICK_FATAL_EXIT("Workloads buffer alignment overflow while sizing stats type '%s'", workload_stats_type->name.c_str());
			if (!increment_workloads_cursor_for_type(*workload_stats_type, total_size))
				ROBOTICK_FATAL_EXIT("Workloads buffer overflow while sizing stats type '%s'", workload_stats_type->name.c_str());

			if (!align_workloads_cursor_for_type(*workload_type, total_size))
				ROBOTICK_FATAL_EXIT("Workloads buffer alignment overflow for workload type '%s'", workload_type->name.c_str());
			if (!increment_workloads_cursor_for_type(*workload_type, total_size))
				ROBOTICK_FATAL_EXIT("Workloads buffer overflow while sizing workload type '%s'", workload_type->name.c_str());
		}

		return total_size;
	}

	void Engine::construct_workload_instances()
	{
		ROBOTICK_ASSERT(state != nullptr);
		ROBOTICK_ASSERT(state->model != nullptr);

		const auto* workload_stats_type = TypeRegistry::get().find_by_id(GET_TYPE_ID(WorkloadInstanceStats));
		ROBOTICK_ASSERT_MSG(workload_stats_type, "Type 'WorkloadInstanceStats' not registered - this should never happen");

		const auto& seeds = state->model->get_workload_seeds();
		size_t workloads_cursor = 0;
		uint8_t* buffer_ptr = state->workloads_buffer.raw_ptr();
		state->instances.initialize(seeds.size());

		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto* seed = seeds[i];

			const auto* workload_type = TypeRegistry::get().find_by_id(seed->type_id);
			ROBOTICK_ASSERT_MSG(workload_type, "Unknown workload type: %s", seed->type_id.get_debug_name());

			const auto* workload_desc = workload_type->get_workload_desc();
			ROBOTICK_ASSERT(workload_desc != nullptr);

			if (!align_workloads_cursor_for_type(*workload_stats_type, workloads_cursor))
				ROBOTICK_FATAL_EXIT("Workloads buffer alignment overflow while laying-out stats for workload type '%s'", workload_type->name.c_str());
			const size_t stats_offset = workloads_cursor;
			if (!increment_workloads_cursor_for_type(*workload_stats_type, workloads_cursor))
				ROBOTICK_FATAL_EXIT("Workloads buffer overflow while laying-out stats for workload type '%s'", workload_type->name.c_str());

			if (!align_workloads_cursor_for_type(*workload_type, workloads_cursor))
				ROBOTICK_FATAL_EXIT("Workloads buffer alignment overflow while laying-out workload type '%s'", workload_type->name.c_str());
			const size_t instance_offset = workloads_cursor;
			if (!increment_workloads_cursor_for_type(*workload_type, workloads_cursor))
				ROBOTICK_FATAL_EXIT("Workloads buffer overflow while laying-out workload type '%s'", workload_type->name.c_str());

			uint8_t* workload_stats_ptr = buffer_ptr + stats_offset;
			uint8_t* workload_ptr = buffer_ptr + instance_offset;

			WorkloadInstanceInfo& workload_instance_info = state->instances[i];
			workload_instance_info.offset_in_workloads_buffer = instance_offset;
			workload_instance_info.type = workload_type;
			workload_instance_info.workload_descriptor = workload_desc;
			workload_instance_info.seed = seed;

			workload_instance_info.workload_stats = new (static_cast<void*>(workload_stats_ptr)) WorkloadInstanceStats{};
			workload_instance_info.workload_stats->tick_rate_hz = seed->tick_rate_hz;

			state->instances_by_unique_name.insert(seed->unique_name.c_str(), &workload_instance_info);

			if (workload_desc->construct_fn)
			{
				workload_desc->construct_fn(workload_ptr);
			}
		}
	}

	void Engine::run_workload_pre_load_pass()
	{
		ROBOTICK_ASSERT(state != nullptr);
		ROBOTICK_ASSERT(state->model != nullptr);

		const auto& seeds = state->model->get_workload_seeds();
		uint8_t* buffer_ptr = state->workloads_buffer.raw_ptr();
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

				const bool fatalExitIfNotFound = false;
				DataConnectionUtils::apply_struct_field_values(
					ptr + workload_desc->config_offset, *workload_desc->config_desc, seed->config, fatalExitIfNotFound);
			}

			if (workload_desc->pre_load_fn)
				workload_desc->pre_load_fn(ptr);
		}
	}

	void Engine::destroy_constructed_workload_instances()
	{
		ROBOTICK_ASSERT(state != nullptr);

		for (auto& instance : state->instances)
		{
			if (!instance.workload_descriptor || !instance.workload_descriptor->destruct_fn)
				continue;

			void* instance_ptr = instance.get_ptr(*this);
			ROBOTICK_ASSERT(instance_ptr != nullptr);
			instance.workload_descriptor->destruct_fn(instance_ptr);
		}
	}

	void Engine::reset_loaded_workload_state()
	{
		ROBOTICK_ASSERT(state != nullptr);

		state->root_instance = nullptr;
		state->instances_by_unique_name.clear();
		state->data_connections_all.reset();
		state->data_connections_acquired.reset();
		state->data_connection_input_handles.clear();
		state->data_connection_input_handles_by_path.clear();
		state->data_connection_input_handles_by_dest_ptr.clear();
		state->data_connection_input_handles_by_id.clear();
		state->next_data_connection_input_handle_id = 1;
		state->instances.reset();
		state->workloads_buffer = WorkloadsBuffer();
	}

	size_t Engine::compute_dynamic_struct_storage_requirements(
		const HeapVector<WorkloadInstanceInfo>& instances, size_t& out_dynamic_struct_storage_start_alignment)
	{
		size_t total = 0;
		out_dynamic_struct_storage_start_alignment = 1;
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
					const TypeDescriptor* field_type_desc = field.find_type_descriptor();
					const DynamicStructDescriptor* dynamic_struct_desc = field_type_desc ? field_type_desc->get_dynamic_struct_desc() : nullptr;
					if (!dynamic_struct_desc || !dynamic_struct_desc->has_storage_callbacks())
					{
						continue;
					}

					void* dynamic_struct_ptr = field.get_data_ptr(state->workloads_buffer, instance, *struct_type_desc, struct_offset);
					DynamicStructStoragePlan storage_plan;
					if (!dynamic_struct_desc->plan_storage(dynamic_struct_ptr, storage_plan))
					{
						const char* instance_name =
							(instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
						ROBOTICK_FATAL_EXIT("Dynamic struct storage planning failed for workload '%s' field '%s'", instance_name, field.name.c_str());
					}

					if (storage_plan.alignment > out_dynamic_struct_storage_start_alignment)
					{
						out_dynamic_struct_storage_start_alignment = storage_plan.alignment;
					}

					if (!align_workloads_cursor_with_alignment(storage_plan.alignment, total))
					{
						const char* instance_name =
							(instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
						ROBOTICK_FATAL_EXIT(
							"Dynamic struct storage alignment overflow while sizing workload '%s' field '%s'", instance_name, field.name.c_str());
					}

					size_t next_total = 0;
					if (!safe_add_size(total, storage_plan.size_bytes, next_total))
					{
						const char* instance_name =
							(instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
						ROBOTICK_FATAL_EXIT(
							"Dynamic struct storage overflow while sizing workload '%s' field '%s'", instance_name, field.name.c_str());
					}
					total = next_total;
				}
			};

			accumulate(workload_descriptor->config_desc, workload_descriptor->config_offset);
			accumulate(workload_descriptor->inputs_desc, workload_descriptor->inputs_offset);
			accumulate(workload_descriptor->outputs_desc, workload_descriptor->outputs_offset);
		}
		return total;
	}

	void Engine::bind_dynamic_struct_storage_in_struct(
		WorkloadInstanceInfo& instance, const TypeDescriptor& struct_type_desc, const size_t struct_offset, size_t& dynamic_struct_storage_cursor)
	{
		const StructDescriptor* struct_desc = struct_type_desc.get_struct_desc();
		ROBOTICK_ASSERT(struct_desc != nullptr);

		for (const FieldDescriptor& field : struct_desc->fields)
		{
			const TypeDescriptor* field_type_desc = field.find_type_descriptor();
			const DynamicStructDescriptor* dynamic_struct_desc = field_type_desc ? field_type_desc->get_dynamic_struct_desc() : nullptr;
			if (!dynamic_struct_desc || !dynamic_struct_desc->has_storage_callbacks())
			{
				continue;
			}

			void* dynamic_struct_ptr = field.get_data_ptr(state->workloads_buffer, instance, struct_type_desc, struct_offset);
			DynamicStructStoragePlan storage_plan;
			if (!dynamic_struct_desc->plan_storage(dynamic_struct_ptr, storage_plan))
			{
				const char* instance_name = (instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
				ROBOTICK_FATAL_EXIT(
					"Dynamic struct storage planning failed during bind for workload '%s' field '%s'", instance_name, field.name.c_str());
			}

			if (!align_workloads_cursor_with_alignment(storage_plan.alignment, dynamic_struct_storage_cursor))
			{
				const char* instance_name = (instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
				ROBOTICK_FATAL_EXIT(
					"Dynamic struct storage alignment overflow during bind for workload '%s' field '%s'", instance_name, field.name.c_str());
			}

			const size_t storage_offset = dynamic_struct_storage_cursor;
			size_t next_cursor = 0;
			if (!safe_add_size(dynamic_struct_storage_cursor, storage_plan.size_bytes, next_cursor))
			{
				const char* instance_name = (instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
				ROBOTICK_FATAL_EXIT("Dynamic struct storage overflow during bind for workload '%s' field '%s'", instance_name, field.name.c_str());
			}

			if (!dynamic_struct_desc->bind_storage(dynamic_struct_ptr, state->workloads_buffer, storage_offset, storage_plan.size_bytes))
			{
				const char* instance_name = (instance.seed && instance.seed->unique_name.c_str()) ? instance.seed->unique_name.c_str() : "unknown";
				ROBOTICK_FATAL_EXIT("Dynamic struct storage binding failed for workload '%s' field '%s'", instance_name, field.name.c_str());
			}

			dynamic_struct_storage_cursor = next_cursor;
		}
	}

	void Engine::bind_dynamic_struct_storage_for_instances(HeapVector<WorkloadInstanceInfo>& instances, size_t start_offset)
	{
		for (auto& instance : instances)
		{
			const WorkloadDescriptor* workload_desc = instance.workload_descriptor;
			if (!workload_desc)
				continue;

			if (workload_desc->config_desc)
				bind_dynamic_struct_storage_in_struct(instance, *workload_desc->config_desc, workload_desc->config_offset, start_offset);
			if (workload_desc->inputs_desc)
				bind_dynamic_struct_storage_in_struct(instance, *workload_desc->inputs_desc, workload_desc->inputs_offset, start_offset);
			if (workload_desc->outputs_desc)
				bind_dynamic_struct_storage_in_struct(instance, *workload_desc->outputs_desc, workload_desc->outputs_offset, start_offset);
		}
	}

} // namespace robotick
