// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"

#include "robotick/config/PlatformDefaults.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/Thread.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace robotick
{
	struct Engine::State
	{
		Model m_loaded_model;
		bool is_running = false;

		WorkloadsBuffer workloads_buffer;

		const WorkloadInstanceInfo* root_instance = nullptr;
		std::vector<WorkloadInstanceInfo> instances;
		std::vector<DataConnectionInfo> data_connections_all;
		std::vector<size_t> data_connections_acquired_indices;
	};

	Engine::Engine() : state(std::make_unique<Engine::State>())
	{
	}

	Engine::~Engine()
	{
		for (auto& instance : state->instances)
		{
			if (instance.type->destruct)
			{
				void* instance_ptr = instance.get_ptr(*this);
				assert(instance_ptr != nullptr && "Workload pointer should not be null at this point");

				instance.type->destruct(instance_ptr);
			}
		}
	}

	const WorkloadInstanceInfo* Engine::get_root_instance_info() const
	{
		return state->root_instance;
	}

	const WorkloadInstanceInfo& Engine::get_instance_info(size_t index) const
	{
		return state->instances.at(index);
	}

	const std::vector<WorkloadInstanceInfo>& Engine::get_all_instance_info() const
	{
		return state->instances;
	}

	const std::vector<DataConnectionInfo>& Engine::get_all_data_connections() const
	{
		return state->data_connections_all;
	}

	WorkloadsBuffer& Engine::get_workloads_buffer() const
	{
		return state->workloads_buffer;
	}

	size_t Engine::compute_blackboard_memory_requirements(const std::vector<WorkloadInstanceInfo>& instances)
	{
		// note - this function is not recursive, since we don't expect to have nested blackboards

		size_t total = 0;

		for (const auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			void* instance_ptr = instance.get_ptr(*this);
			assert(instance_ptr != nullptr && "Engine should never have null instances at this point");

			auto accumulate = [&](const StructRegistryEntry* struct_info)
			{
				if (!struct_info)
				{
					return;
				}

				const size_t struct_offset = struct_info->offset_within_workload;

				const auto struct_ptr = static_cast<uint8_t*>(instance_ptr) + struct_offset;

				for (const FieldInfo& field : struct_info->fields)
				{
					if (field.type == typeid(Blackboard))
					{
						assert(field.offset_within_struct != OFFSET_UNBOUND && "Field offset should have been correctly set by now");

						const auto* blackboard_raw_ptr = struct_ptr + field.offset_within_struct;
						const auto* blackboard = reinterpret_cast<const Blackboard*>(blackboard_raw_ptr);

						total += blackboard->get_info()->total_datablock_size;
					}
				}
			};

			accumulate(type->config_struct);
			accumulate(type->input_struct);
			accumulate(type->output_struct);
		}

		return total;
	}

	void Engine::bind_blackboards_in_struct(
		WorkloadInstanceInfo& workload_instance_info, const StructRegistryEntry& struct_entry, size_t& blackboard_storage_offset)
	{
		// note - this function is not recursive, since we don't expect to have nested blackboards

		for (const FieldInfo& field : struct_entry.fields)
		{
			if (field.type == typeid(Blackboard))
			{
				Blackboard& blackboard = field.get_data<Blackboard>(state->workloads_buffer, workload_instance_info, struct_entry);
				blackboard.bind(blackboard_storage_offset);

				blackboard_storage_offset += blackboard.get_info()->total_datablock_size;
			}
		}
	}

	void Engine::bind_blackboards_for_instances(std::vector<WorkloadInstanceInfo>& instances, const size_t blackboards_data_start_offset)
	{
		size_t blackboard_storage_offset = blackboards_data_start_offset;

		for (WorkloadInstanceInfo& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
			{
				continue;
			}

			if (type->config_struct)
			{
				bind_blackboards_in_struct(instance, *type->config_struct, blackboard_storage_offset);
			}

			if (type->input_struct)
			{
				bind_blackboards_in_struct(instance, *type->input_struct, blackboard_storage_offset);
			}

			if (type->output_struct)
			{
				bind_blackboards_in_struct(instance, *type->output_struct, blackboard_storage_offset);
			}
		}
	}

	void Engine::load(const Model& model)
	{
		if (!model.get_root().is_valid())
		{
			throw std::runtime_error("Model has no root workload set via model.set_root(...)");
		}

		state->instances.clear();
		state->data_connections_all.clear();
		state->data_connections_acquired_indices.clear();
		state->root_instance = nullptr;
		state->workloads_buffer = WorkloadsBuffer();
		state->m_loaded_model = model;

		const auto& workload_seeds = model.get_workload_seeds();

		// compute how much room we need in our WorkloadsBuffer, and required offset for each workload object
		size_t workloads_buffer_cursor = 0;
		std::vector<size_t> aligned_offsets;
		for (const auto& workload_seed : workload_seeds)
		{
			const auto* type = WorkloadRegistry::get().find(workload_seed.type);
			if (!type)
				throw std::runtime_error("Unknown workload type: " + workload_seed.type);

			const size_t alignment = std::max<size_t>(type->alignment, alignof(std::max_align_t));
			workloads_buffer_cursor = (workloads_buffer_cursor + alignment - 1) & ~(alignment - 1);
			aligned_offsets.push_back(workloads_buffer_cursor);
			workloads_buffer_cursor += type->size;
		}

		const size_t all_workloads_size = workloads_buffer_cursor;

		// TODO: [#88] Temporary allocation strategy: pre-allocates workloads and an overestimated blackboards size.
		// This will be replaced with a multi-stripe allocation model as outlined in ticket #88
		state->workloads_buffer = WorkloadsBuffer(all_workloads_size + DEFAULT_MAX_BLACKBOARDS_BYTES);
		uint8_t* workloads_buffer_ptr = state->workloads_buffer.raw_ptr();

		// construct and add instances
		state->instances.reserve(workload_seeds.size());

		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			const auto& workload_seed = workload_seeds[i];
			const auto* type = WorkloadRegistry::get().find(workload_seed.type);

			const size_t instance_offset = aligned_offsets[i];
			uint8_t* instance_ptr = workloads_buffer_ptr + instance_offset;

			if (type->construct)
				type->construct(instance_ptr);

			state->instances.push_back(
				WorkloadInstanceInfo{instance_offset, type, workload_seed.name, workload_seed.tick_rate_hz, {}, WorkloadInstanceStats{}});
		}

		// configure and pre-load in parallel
		std::vector<std::future<void>> preload_futures;
		preload_futures.reserve(workload_seeds.size());

		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			const auto& workload_seed = workload_seeds[i];
			const auto* type = state->instances[i].type;
			uint8_t* instance_ptr = workloads_buffer_ptr + state->instances[i].offset_in_workloads_buffer;

			preload_futures.push_back(std::async(std::launch::async,
				[=, this]()
				{
					if (type->set_engine_fn)
						type->set_engine_fn(instance_ptr, *this);

					if (type->config_struct)
					{
						assert(type->config_struct->offset_within_workload != OFFSET_UNBOUND && "Struct offset not initialized");
						apply_struct_fields(instance_ptr + type->config_struct->offset_within_workload, *type->config_struct, workload_seed.config);
					}

					if (type->pre_load_fn)
						type->pre_load_fn(instance_ptr);
				}));
		}

		// Wait for all parallel setup work to finish
		for (auto& fut : preload_futures)
			fut.get();

		// Blackboards memory allocation and buffer-binding:
		const size_t blackboards_buffer_size = compute_blackboard_memory_requirements(state->instances);
		if (blackboards_buffer_size > robotick::DEFAULT_MAX_BLACKBOARDS_BYTES)
		{
			std::ostringstream msg;
			msg << "[Engine::load] Required blackboard memory (" << blackboards_buffer_size << " bytes) exceeds platform default maximum ("
				<< robotick::DEFAULT_MAX_BLACKBOARDS_BYTES << " bytes).\n"
				<< "Increase DEFAULT_MAX_BLACKBOARDS_BYTES or allocate explicitly for your platform.";

			throw std::runtime_error(msg.str());
		}

		if (blackboards_buffer_size > 0)
		{
			const size_t blackboards_data_start_offset = workloads_buffer_cursor;
			workloads_buffer_cursor += blackboards_buffer_size;

			assert(blackboards_data_start_offset >= all_workloads_size && "Blackboards data should all appear after workloads-data in buffer");

			bind_blackboards_for_instances(state->instances, blackboards_data_start_offset);
		}

		// call load_fn (multi-threaded):
		std::vector<std::future<void>> load_futures;
		load_futures.reserve(workload_seeds.size());

		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			void* instance_ptr = state->instances[i].get_ptr(*this);
			const auto* type = state->instances[i].type;

			load_futures.push_back(std::async(std::launch::async,
				[=]()
				{
					if (type->load_fn)
						type->load_fn(instance_ptr);
				}));
		}

		// wait for "load" futures to complete:
		for (auto& fut : load_futures)
		{
			fut.get();
		}

		// Setup data connections from the model and store them in the engine state.
		// We also collect pointers to all connections that still need to be acquired
		// by a workload (typically handled by Grouped Workloads via set_children_fn() below).
		state->data_connections_all = DataConnectionsFactory::create(state->workloads_buffer, model.get_data_connection_seeds(), state->instances);
		std::vector<DataConnectionInfo*> data_connections_pending_acquisition;
		for (auto& conn : state->data_connections_all)
		{
			data_connections_pending_acquisition.push_back(&conn);
		}

		// fixup child-instances:
		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			const auto& workload_seed = workload_seeds[i];
			auto& instance = state->instances[i];

			assert(instance.unique_name == workload_seed.name && "[Engine] Workload instances should be in same order as seeds (different names)");
			assert(instance.type->name == workload_seed.type && "[Engine] Workload instances should be in same order as seeds (different types");

			if (instance.type->set_children_fn)
			{
				for (auto child_handle : workload_seed.children)
				{
					const WorkloadInstanceInfo& child_workload = get_instance_info(child_handle.index);
					instance.children.push_back(&child_workload);
				}
			}
		}

		// call set_children_fn on root-instance - we expect that to recursively do so for any children
		// allowing them to set up their children, acquire responsibility for data-connections, etc as needed.
		const WorkloadInstanceInfo* root_instance = &get_instance_info(model.get_root().index);
		assert(root_instance != nullptr);

		if (root_instance && root_instance->type && root_instance->type->set_children_fn)
		{
			root_instance->type->set_children_fn(root_instance->get_ptr(*this), root_instance->children, data_connections_pending_acquisition);
		}

		// acquire any pending data_connections have been requested for engine-handling - these should be all that remain:
		auto new_end = std::remove_if(data_connections_pending_acquisition.begin(), data_connections_pending_acquisition.end(),
			[this](const DataConnectionInfo* conn)
			{
				if (conn->expected_handler == DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine)
				{
					state->data_connections_acquired_indices.push_back(static_cast<size_t>(conn - state->data_connections_all.data()));
					return true;
				}
				return false;
			});
		data_connections_pending_acquisition.erase(new_end, data_connections_pending_acquisition.end());

		// validate that all pending data_connections have been acquired by workloads:
		if (!data_connections_pending_acquisition.empty())
		{
			std::ostringstream msg;
			msg << "Error: Not all data connections were acquired by workloads or engine.\n";
			msg << "Unclaimed connections (" << data_connections_pending_acquisition.size() << "):\n";

			for (const DataConnectionInfo* conn : data_connections_pending_acquisition)
			{
				msg << "  - " << conn->seed.source_field_path << " -> " << conn->seed.dest_field_path << '\n';
			}

			throw std::runtime_error(msg.str());
		}

		// setup each instance
		for (auto& instance : state->instances)
		{
			if (instance.type->setup_fn)
			{
				void* instance_ptr = instance.get_ptr(*this);
				assert(instance_ptr != nullptr && "Workload pointer should not be null at this point");

				instance.type->setup_fn(instance_ptr);
			}
		}

		state->root_instance = root_instance;
		assert(state->root_instance != nullptr);
	}

	bool Engine::is_running() const
	{
		return state != nullptr && state->is_running;
	}

	void Engine::run(const std::atomic<bool>& stop_after_next_tick_flag)
	{
		using namespace std::chrono;

		const WorkloadHandle root_handle = state->m_loaded_model.get_root();
		if (root_handle.index >= state->instances.size())
			throw std::runtime_error("Invalid root workload handle");

		const auto& root_info = state->instances[root_handle.index];
		void* root_ptr = root_info.get_ptr(*this);

		if (root_info.tick_rate_hz <= 0 || !root_info.type->tick_fn || !root_ptr)
			throw std::runtime_error("Root workload must have valid tick_rate_hz and tick()");

		// call start() on all workloads
		for (auto& inst : state->instances)
		{
			if (inst.type->start_fn)
			{
				inst.type->start_fn(inst.get_ptr(*this), inst.tick_rate_hz);
			}
		}

		state->is_running = true; // flag that we're running - e.g. allows this to be seen in telemetry / asserts

		const auto tick_interval_sec = duration<double>(1.0 / root_info.tick_rate_hz);
		const auto tick_interval = duration_cast<steady_clock::duration>(tick_interval_sec);
		auto next_tick_time = steady_clock::now();
		auto last_tick_time = steady_clock::now();

		// main tick-loop: (tick at least once, before checking stop_after_next_tick_flag - e.g. useful for testing)
		do
		{
			const auto now_pre_tick = steady_clock::now();
			const double time_delta = duration<double>(now_pre_tick - last_tick_time).count();
			last_tick_time = now_pre_tick;

			// update any data-connections owned by the engine:
			for (size_t index : state->data_connections_acquired_indices)
			{
				state->data_connections_all[index].do_data_copy();
			}

			// Ensure all data writes complete before tick begins
			std::atomic_thread_fence(std::memory_order_release);

			// tick root (will cause children to tick recursively)
			root_info.type->tick_fn(root_ptr, time_delta);

			const auto now_post_tick = steady_clock::now();
			root_info.mutable_stats.last_tick_duration = duration<double>(now_post_tick - now_pre_tick).count();

			root_info.mutable_stats.last_time_delta = time_delta;
			next_tick_time += tick_interval;

			hybrid_sleep_until(time_point_cast<steady_clock::duration>(next_tick_time));

		} while (!stop_after_next_tick_flag);

		state->is_running = false; // flag that we're no longer running - e.g. allows this to be seen in telemetry / asserts

		// call stop() on all children (do it safely here, rather than relying on stop() to propagate through hierarchy)
		for (auto& inst : state->instances)
		{
			if (inst.type->stop_fn)
			{
				inst.type->stop_fn(inst.get_ptr(*this));
			}
		}
	}

} // namespace robotick
