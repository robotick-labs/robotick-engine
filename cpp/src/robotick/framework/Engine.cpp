// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"

#include "robotick/api.h"
#include "robotick/config/PlatformDefaults.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
#include "robotick/framework/data/WorkloadsBuffer.h"
#include "robotick/framework/registry/FieldRegistry.h"
#include "robotick/framework/registry/FieldUtils.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/TypeId.h"
#include "robotick/platform/Threading.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <sstream>
#include <stdexcept>
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
				ROBOTICK_ASSERT(instance_ptr != nullptr);
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
		size_t total = 0;
		for (const auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			void* instance_ptr = instance.get_ptr(*this);
			ROBOTICK_ASSERT(instance_ptr != nullptr);

			auto accumulate = [&](const StructRegistryEntry* struct_info)
			{
				if (!struct_info)
					return;
				const size_t struct_offset = struct_info->offset_within_workload;
				const auto struct_ptr = static_cast<uint8_t*>(instance_ptr) + struct_offset;

				for (const FieldInfo& field : struct_info->fields)
				{
					if (field.type == GET_TYPE_ID(Blackboard))
					{
						ROBOTICK_ASSERT(field.offset_within_struct != OFFSET_UNBOUND);
						const auto* blackboard = reinterpret_cast<const Blackboard*>(struct_ptr + field.offset_within_struct);
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

	void Engine::bind_blackboards_in_struct(WorkloadInstanceInfo& instance, const StructRegistryEntry& struct_entry, size_t& offset)
	{
		for (const FieldInfo& field : struct_entry.fields)
		{
			if (field.type == GET_TYPE_ID(Blackboard))
			{
				Blackboard& blackboard = field.get_data<Blackboard>(state->workloads_buffer, instance, struct_entry);
				blackboard.bind(offset);
				offset += blackboard.get_info()->total_datablock_size;
			}
		}
	}

	void Engine::bind_blackboards_for_instances(std::vector<WorkloadInstanceInfo>& instances, size_t start_offset)
	{
		for (auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			if (type->config_struct)
				bind_blackboards_in_struct(instance, *type->config_struct, start_offset);
			if (type->input_struct)
				bind_blackboards_in_struct(instance, *type->input_struct, start_offset);
			if (type->output_struct)
				bind_blackboards_in_struct(instance, *type->output_struct, start_offset);
		}
	}

	void Engine::load(const Model& model)
	{
		if (!model.get_root().is_valid())
			ROBOTICK_ERROR("Model has no root workload");

		state->instances.clear();
		state->data_connections_all.clear();
		state->data_connections_acquired_indices.clear();
		state->root_instance = nullptr;
		state->m_loaded_model = model;

		const auto& seeds = model.get_workload_seeds();
		size_t workloads_cursor = 0;
		std::vector<size_t> offsets;
		for (const auto& seed : seeds)
		{
			const auto* type = WorkloadRegistry::get().find(seed.type.c_str());
			if (!type)
				ROBOTICK_ERROR("Unknown workload type: %s", seed.type.c_str());

			const size_t align = std::max<size_t>(type->alignment, alignof(std::max_align_t));
			workloads_cursor = (workloads_cursor + align - 1) & ~(align - 1);
			offsets.push_back(workloads_cursor);
			workloads_cursor += type->size;
		}

		const size_t total_size = workloads_cursor;
		state->workloads_buffer = WorkloadsBuffer(total_size + DEFAULT_MAX_BLACKBOARDS_BYTES);
		uint8_t* buffer_ptr = state->workloads_buffer.raw_ptr();
		state->instances.reserve(seeds.size());

		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto& seed = seeds[i];
			const auto* type = WorkloadRegistry::get().find(seed.type.c_str());
			uint8_t* ptr = buffer_ptr + offsets[i];

			state->instances.emplace_back(WorkloadInstanceInfo{offsets[i], type, seed.name, seed.tick_rate_hz, {}, WorkloadInstanceStats{}});
#if defined(ROBOTICK_DEBUG)
			constexpr size_t MAX_SIZE = 2048;
			alignas(std::max_align_t) static constexpr uint8_t zero[MAX_SIZE] = {};
			ROBOTICK_ASSERT(type->size <= MAX_SIZE);
			ROBOTICK_ASSERT(std::memcmp(ptr, zero, type->size) == 0);
#endif
			if (type->construct)
				type->construct(ptr);
		}

		std::vector<std::future<void>> preload_futures;
		for (size_t i = 0; i < seeds.size(); ++i)
		{
			const auto& seed = seeds[i];
			const auto* type = state->instances[i].type;
			uint8_t* ptr = buffer_ptr + state->instances[i].offset_in_workloads_buffer;

			preload_futures.emplace_back(std::async(std::launch::async,
				[=, this]()
				{
					if (type->set_engine_fn)
						type->set_engine_fn(ptr, *this);
					if (type->config_struct)
						apply_struct_fields(ptr + type->config_struct->offset_within_workload, *type->config_struct, seed.config);
					if (type->pre_load_fn)
						type->pre_load_fn(ptr);
				}));
		}
		for (auto& fut : preload_futures)
			fut.get();

		size_t blackboard_size = compute_blackboard_memory_requirements(state->instances);
		if (blackboard_size > DEFAULT_MAX_BLACKBOARDS_BYTES)
			ROBOTICK_ERROR("Blackboard memory (%zu) exceeds max allowed (%zu)", blackboard_size, DEFAULT_MAX_BLACKBOARDS_BYTES);

		if (blackboard_size > 0)
		{
			size_t start = workloads_cursor;
			workloads_cursor += blackboard_size;
			bind_blackboards_for_instances(state->instances, start);
		}

		std::vector<std::future<void>> load_futures;
		for (auto& inst : state->instances)
		{
			void* ptr = inst.get_ptr(*this);
			const auto* type = inst.type;
			load_futures.emplace_back(std::async(std::launch::async,
				[ptr, type]()
				{
					if (type->load_fn)
						type->load_fn(ptr);
				}));
		}
		for (auto& fut : load_futures)
			fut.get();

		state->data_connections_all = DataConnectionsFactory::create(state->workloads_buffer, model.get_data_connection_seeds(), state->instances);
		std::vector<DataConnectionInfo*> pending;
		for (auto& conn : state->data_connections_all)
			pending.push_back(&conn);

		for (size_t i = 0; i < seeds.size(); ++i)
		{
			auto& inst = state->instances[i];
			const auto& seed = seeds[i];

			if (inst.type->set_children_fn)
			{
				for (auto child_handle : seed.children)
					inst.children.push_back(&get_instance_info(child_handle.index));
			}
		}

		const auto* root_instance = &get_instance_info(model.get_root().index);
		ROBOTICK_ASSERT(root_instance != nullptr);
		if (root_instance->type->set_children_fn)
			root_instance->type->set_children_fn(root_instance->get_ptr(*this), root_instance->children, pending);

		auto new_end = std::remove_if(pending.begin(), pending.end(),
			[this](DataConnectionInfo* conn)
			{
				if (conn->expected_handler == DataConnectionInfo::ExpectedHandler::ParentGroupOrEngine)
				{
					state->data_connections_acquired_indices.push_back(conn - state->data_connections_all.data());
					return true;
				}
				return false;
			});
		pending.erase(new_end, pending.end());

		if (!pending.empty())
		{
			const auto* conn = pending.front();
			ROBOTICK_ERROR("Unclaimed connection: %s -> %s", conn->seed.source_field_path.c_str(), conn->seed.dest_field_path.c_str());
		}

		for (auto& inst : state->instances)
		{
			if (inst.type->setup_fn)
				inst.type->setup_fn(inst.get_ptr(*this));
		}

		state->root_instance = root_instance;
	}

	bool Engine::is_running() const
	{
		return state && state->is_running;
	}

	void Engine::run(const AtomicFlag& stop_after_next_tick_flag)
	{
		const WorkloadHandle root_handle = state->m_loaded_model.get_root();
		if (root_handle.index >= state->instances.size())
			ROBOTICK_ERROR("Invalid root workload handle");

		const auto& root_info = state->instances[root_handle.index];
		void* root_ptr = root_info.get_ptr(*this);

		if (root_info.tick_rate_hz <= 0 || !root_info.type->tick_fn || !root_ptr)
			ROBOTICK_ERROR("Root workload must have valid tick_rate_hz and tick_fn");

		// Start all workloads
		for (auto& inst : state->instances)
		{
			if (inst.type->start_fn)
				inst.type->start_fn(inst.get_ptr(*this), inst.tick_rate_hz);
		}

		state->is_running = true;

		const std::chrono::duration<double> tick_interval_sec(1.0 / root_info.tick_rate_hz);
		const std::chrono::steady_clock::duration tick_interval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(tick_interval_sec);

		std::chrono::steady_clock::time_point next_tick_time = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point last_tick_time = next_tick_time;

		do
		{
			const std::chrono::steady_clock::time_point now_pre_tick = std::chrono::steady_clock::now();
			const double time_delta = std::chrono::duration<double>(now_pre_tick - last_tick_time).count();
			last_tick_time = now_pre_tick;

			for (size_t index : state->data_connections_acquired_indices)
				state->data_connections_all[index].do_data_copy();

			std::atomic_thread_fence(std::memory_order_release);

			root_info.type->tick_fn(root_ptr, time_delta);

			const std::chrono::steady_clock::time_point now_post_tick = std::chrono::steady_clock::now();
			root_info.mutable_stats.last_tick_duration = std::chrono::duration<double>(now_post_tick - now_pre_tick).count();
			root_info.mutable_stats.last_time_delta = time_delta;

			next_tick_time += tick_interval;
			Thread::hybrid_sleep_until(std::chrono::time_point_cast<std::chrono::steady_clock::duration>(next_tick_time));

		} while (!stop_after_next_tick_flag.is_set());

		state->is_running = false;

		for (auto& inst : state->instances)
		{
			if (inst.type->stop_fn)
				inst.type->stop_fn(inst.get_ptr(*this));
		}
	}

} // namespace robotick
