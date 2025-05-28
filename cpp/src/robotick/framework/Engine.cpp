// Copyright Robotick Labs
//
// SPDX-License-Identifier: Apache-2.0

#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/data/Blackboard.h"
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
#include <stdexcept>
#include <thread>
#include <vector>

namespace robotick
{

	struct Engine::Impl
	{
		const WorkloadInstanceInfo* root_instance = nullptr;
		std::vector<WorkloadInstanceInfo> instances;
		Model m_loaded_model;
		uint8_t* buffer = nullptr;
		size_t buffer_size = 0;
		uint8_t* blackboards_buffer = nullptr;
		size_t blackboards_buffer_size = 0;
	};

	Engine::Engine() : m_impl(std::make_unique<Impl>())
	{
	}

	Engine::~Engine()
	{
		for (auto& instance : m_impl->instances)
		{
			if (instance.type->destruct)
				instance.type->destruct(instance.ptr);
		}

		delete[] m_impl->buffer;
		delete[] m_impl->blackboards_buffer;
	}

	const WorkloadInstanceInfo& Engine::get_instance_info(size_t index) const
	{
		return m_impl->instances.at(index);
	}

	size_t compute_blackboard_memory_requirements(const std::vector<WorkloadInstanceInfo>& instances)
	{
		size_t total = 0;

		for (const auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			auto accumulate = [&](void* instance_ptr, const StructRegistryEntry* struct_info, const size_t struct_offset)
			{
				if (!struct_info)
					return;

				const auto struct_ptr = static_cast<uint8_t*>(instance_ptr) + struct_offset;

				for (const FieldInfo& field : struct_info->fields)
				{
					if (field.type == typeid(Blackboard))
					{
						const auto* blackboard_raw_ptr = struct_ptr + field.offset;
						const auto* blackboard = reinterpret_cast<const Blackboard*>(blackboard_raw_ptr);

						if (blackboard && !blackboard->get_schema().empty())
						{
							total += blackboard->required_size();
						}
					}
				}
			};

			accumulate(instance.ptr, type->config_struct, type->config_offset);
			accumulate(instance.ptr, type->input_struct, type->input_offset);
			accumulate(instance.ptr, type->output_struct, type->output_offset);
		}

		return total;
	}

	void bind_blackboards_in_struct(
		void* instance_ptr, const StructRegistryEntry& struct_entry, const size_t struct_offset, uint8_t*& blackboard_storage)
	{
		for (const FieldInfo& field : struct_entry.fields)
		{
			if (field.type == typeid(Blackboard))
			{
				auto* blackboard = reinterpret_cast<Blackboard*>(static_cast<uint8_t*>(instance_ptr) + struct_offset + field.offset);
				if (blackboard && !blackboard->get_schema().empty())
				{
					blackboard->bind(blackboard_storage);
					blackboard_storage += blackboard->required_size();
				}
			}
		}
	}

	void bind_blackboards_for_instances(std::vector<WorkloadInstanceInfo>& instances, uint8_t* blackboards_buffer)
	{
		uint8_t* blackboard_storage = blackboards_buffer;

		for (const auto& instance : instances)
		{
			const auto* type = instance.type;
			if (!type)
				continue;

			if (type->config_struct)
				bind_blackboards_in_struct(instance.ptr, *type->config_struct, type->config_offset, blackboard_storage);
			if (type->input_struct)
				bind_blackboards_in_struct(instance.ptr, *type->input_struct, type->input_offset, blackboard_storage);
			if (type->output_struct)
				bind_blackboards_in_struct(instance.ptr, *type->output_struct, type->output_offset, blackboard_storage);
		}
	}

	void Engine::load(const Model& model)
	{
		if (!model.get_root().is_valid())
		{
			throw std::runtime_error("Model has no root workload set via model.set_root(...)");
		}

		m_impl->instances.clear();
		delete[] m_impl->buffer;
		m_impl->buffer = nullptr;
		m_impl->buffer_size = 0;
		m_impl->m_loaded_model = model;

		const auto& workload_seeds = model.get_workload_seeds();

		size_t offset = 0;
		std::vector<size_t> aligned_offsets;
		for (const auto& workload_seed : workload_seeds)
		{
			const auto* type = WorkloadRegistry::get().find(workload_seed.type);
			if (!type)
				throw std::runtime_error("Unknown workload type: " + workload_seed.type);

			size_t alignment = std::max<size_t>(type->alignment, alignof(std::max_align_t));
			offset = (offset + alignment - 1) & ~(alignment - 1);
			aligned_offsets.push_back(offset);
			offset += type->size;
		}

		// Primary workloads buffer allocation:
		m_impl->buffer_size = offset;
		m_impl->buffer = new uint8_t[m_impl->buffer_size];
		std::memset(m_impl->buffer, 0, m_impl->buffer_size);

		std::vector<std::future<WorkloadInstanceInfo>> preload_futures;
		preload_futures.reserve(workload_seeds.size());

		// construct + apply config + call pre_load_fn (multi-threaded):
		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			const auto& workload_seed = workload_seeds[i];
			const auto* type = WorkloadRegistry::get().find(workload_seed.type);
			void* instance_ptr = m_impl->buffer + aligned_offsets[i];

			preload_futures.push_back(std::async(std::launch::async,
				[=]() -> WorkloadInstanceInfo
				{
					if (type->construct)
						type->construct(instance_ptr);

					if (type->config_struct)
					{
						apply_struct_fields(static_cast<uint8_t*>(instance_ptr) + type->config_offset, *type->config_struct, workload_seed.config);
					}

					if (type->pre_load_fn)
						type->pre_load_fn(instance_ptr);

					return WorkloadInstanceInfo{instance_ptr, type, workload_seed.name, workload_seed.tick_rate_hz, {}};
				}));
		}

		// wait for "pre_load" futures to complete and collect results:
		m_impl->instances.reserve(workload_seeds.size());
		for (auto& fut : preload_futures)
		{
			m_impl->instances.push_back(fut.get());
		}

		// Blackboards memory allocation and buffer-binding:
		m_impl->blackboards_buffer_size = compute_blackboard_memory_requirements(m_impl->instances);
		m_impl->blackboards_buffer = new uint8_t[m_impl->blackboards_buffer_size];
		std::memset(m_impl->blackboards_buffer, 0, m_impl->blackboards_buffer_size);

		bind_blackboards_for_instances(m_impl->instances, m_impl->blackboards_buffer);

		// call load_fn (multi-threaded):
		std::vector<std::future<void>> load_futures;
		load_futures.reserve(workload_seeds.size());

		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			void* instance_ptr = m_impl->instances[i].ptr;
			const auto* type = m_impl->instances[i].type;

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

		// fixup and add child-instances
		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			const auto& workload_seed = workload_seeds[i];
			auto& instance = m_impl->instances[i];

			assert(instance.unique_name == workload_seed.name && "[Engine] Workload instances should be in same order as seeds (different names)");
			assert(instance.type->name == workload_seed.type && "[Engine] Workload instances should be in same order as seeds (different types");

			if (instance.type->set_children_fn)
			{
				std::vector<const WorkloadInstanceInfo*> children;
				for (auto child_handle : workload_seed.children)
				{
					const WorkloadInstanceInfo& child_workload = get_instance_info(child_handle.index);
					children.push_back(&child_workload);
				}
				instance.type->set_children_fn(instance.ptr, children);
			}
		}

		// setup each instance
		for (auto& instance : m_impl->instances)
		{
			if (instance.type->setup_fn)
				instance.type->setup_fn(instance.ptr);
		}

		m_impl->root_instance = &get_instance_info(model.get_root().index);
		assert(m_impl->root_instance != nullptr);
	}

	void Engine::run(const std::atomic<bool>& stop_after_next_tick_flag)
	{
		using namespace std::chrono;

		const WorkloadHandle root_handle = m_impl->m_loaded_model.get_root();
		if (root_handle.index >= m_impl->instances.size())
			throw std::runtime_error("Invalid root workload handle");

		const auto& root_info = m_impl->instances[root_handle.index];

		if (root_info.tick_rate_hz <= 0 || !root_info.type->tick_fn || !root_info.ptr)
			throw std::runtime_error("Root workload must have valid tick_rate_hz and tick()");

		// call start() on all workloads
		for (auto& inst : m_impl->instances)
		{
			if (inst.type->start_fn)
			{
				inst.type->start_fn(inst.ptr, inst.tick_rate_hz);
			}
		}

		const auto tick_interval_sec = duration<double>(1.0 / root_info.tick_rate_hz);
		const auto tick_interval = duration_cast<steady_clock::duration>(tick_interval_sec);
		auto next_tick_time = steady_clock::now();
		auto last_tick_time = steady_clock::now();

		// main tick-loop: (tick at least once, before checking stop_after_next_tick_flag - e.g. useful for testing)
		do
		{
			auto now = steady_clock::now();
			double time_delta = duration<double>(now - last_tick_time).count();
			last_tick_time = now;

			root_info.type->tick_fn(root_info.ptr, time_delta);
			next_tick_time += tick_interval;

			hybrid_sleep_until(time_point_cast<steady_clock::duration>(next_tick_time));

		} while (!stop_after_next_tick_flag);

		// call stop() on all children (do it safely here, rather than relying on stop() to propagate through hierarchy)
		for (auto& inst : m_impl->instances)
		{
			if (inst.type->stop_fn)
			{
				inst.type->stop_fn(inst.ptr);
			}
		}
	}

} // namespace robotick
