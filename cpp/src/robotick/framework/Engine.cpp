#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
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
	}

	const WorkloadInstanceInfo& Engine::get_instance_info(size_t index) const
	{
		return m_impl->instances.at(index);
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

		m_impl->buffer_size = offset;
		m_impl->buffer = new uint8_t[m_impl->buffer_size];
		std::memset(m_impl->buffer, 0, m_impl->buffer_size);

		std::vector<std::future<WorkloadInstanceInfo>> futures;
		futures.reserve(workload_seeds.size());

		for (size_t i = 0; i < workload_seeds.size(); ++i)
		{
			const auto& workload_seed = workload_seeds[i];
			const auto* type = WorkloadRegistry::get().find(workload_seed.type);
			void* instance_ptr = m_impl->buffer + aligned_offsets[i];

			futures.push_back(std::async(std::launch::async,
				[=]() -> WorkloadInstanceInfo
				{
					if (type->construct)
						type->construct(instance_ptr);

					if (type->config_struct)
						apply_struct_fields(static_cast<uint8_t*>(instance_ptr) + type->config_offset, *type->config_struct, workload_seed.config);

					if (type->pre_load_fn)
						type->pre_load_fn(instance_ptr);
					if (type->load_fn)
						type->load_fn(instance_ptr);

					const std::vector<const WorkloadInstanceInfo*> children = {}; // fixup add children below once we've created all instances
					return WorkloadInstanceInfo{instance_ptr, type, workload_seed.name, workload_seed.tick_rate_hz, children};
				}));
		}

		for (auto& fut : futures)
		{
			m_impl->instances.push_back(fut.get());
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

	void Engine::run(const std::atomic<bool>& stop_flag)
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

		// main tick-loop: (tick at least once, before checking stop_flag - e.g. useful for testing)
		do
		{
			auto now = steady_clock::now();
			double time_delta = duration<double>(now - last_tick_time).count();
			last_tick_time = now;

			root_info.type->tick_fn(root_info.ptr, time_delta);
			next_tick_time += tick_interval;

			hybrid_sleep_until(time_point_cast<steady_clock::duration>(next_tick_time));

		} while (!stop_flag);

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
