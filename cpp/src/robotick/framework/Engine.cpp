// Engine.cpp
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/registry/WorkloadRegistry.h"
#include "robotick/framework/utils/Thread.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace robotick
{
    struct WorkloadInstance
    {
        void*                        ptr;
        const WorkloadRegistryEntry* type;
        std::string                  unique_name;
        double                       tick_rate_hz;
    };

    struct Engine::Impl
    {
        std::vector<WorkloadInstance> instances;
        std::vector<std::thread>      threads;
        std::atomic<bool>             stop_flag = false;

        uint8_t* buffer = nullptr;
        size_t   buffer_size = 0;
    };

    Engine::Engine() : m_impl(std::make_unique<Impl>())
    {
    }

    Engine::~Engine()
    {
        stop();

        for (auto& instance : m_impl->instances)
        {
            if (instance.type->destruct_fn) instance.type->destruct_fn(instance.ptr);
        }

        delete[] m_impl->buffer;
    }

    void Engine::load(const Model& model)
    {
        m_impl->instances.clear();
        delete[] m_impl->buffer;
        m_impl->buffer = nullptr;
        m_impl->buffer_size = 0;

        const auto& configs = model.get_configs();

        // First pass: calculate total size with alignment padding
        size_t              offset = 0;
        std::vector<size_t> aligned_offsets;

        for (const auto& config : configs)
        {
            const auto* type = WorkloadRegistry::get().find(config.type);
            if (!type) throw std::runtime_error("Unknown workload type: " + config.type);

            size_t alignment = std::max<size_t>(type->alignment, alignof(std::max_align_t));
            offset = (offset + alignment - 1) & ~(alignment - 1);  // align
            aligned_offsets.push_back(offset);
            offset += type->size;
        }

        m_impl->buffer_size = offset;
        m_impl->buffer = new uint8_t[m_impl->buffer_size];
        std::memset(m_impl->buffer, 0, m_impl->buffer_size);

        // Second pass: instantiate in-place
        for (size_t i = 0; i < configs.size(); ++i)
        {
            const auto& config = configs[i];
            const auto* type = WorkloadRegistry::get().find(config.type);

            void* ptr = m_impl->buffer + aligned_offsets[i];

            if (type->construct_fn) type->construct_fn(ptr);
            if (type->config_fn) type->config_fn(ptr, config.config);
            if (type->pre_load_fn) type->pre_load_fn(ptr);
            if (type->load_fn) type->load_fn(ptr);

            m_impl->instances.push_back({ptr, type, config.name, config.tick_rate_hz});
        }
    }

    void Engine::setup()
    {
        for (auto& instance : m_impl->instances)
        {
            if (instance.type->setup_fn) instance.type->setup_fn(instance.ptr);
        }
    }

    static void hybrid_sleep_until(std::chrono::steady_clock::time_point target_time)
    {
        using namespace std::chrono;
        constexpr auto coarse_margin = 500us;
        constexpr auto coarse_step = 100us;

        auto now = steady_clock::now();
        while (now < target_time - coarse_margin)
        {
            std::this_thread::sleep_for(coarse_step);
            now = steady_clock::now();
        }

        while (steady_clock::now() < target_time)
        {
        }
    }

    void Engine::start()
    {
        m_impl->stop_flag = false;

        for (auto& instance : m_impl->instances)
        {
            if (instance.tick_rate_hz <= 0 || !instance.type->tick_fn || !instance.ptr) continue;

            m_impl->threads.emplace_back(
                [this, &instance]()
                {
                    using namespace std::chrono;
                    set_thread_affinity(2);
                    set_thread_priority_high();
                    set_thread_name("robotick_" + std::string(instance.type->name) + "_" + instance.unique_name);

                    const auto tick_interval = duration<double>(1.0 / instance.tick_rate_hz);
                    auto       next_tick_time = steady_clock::now() + tick_interval;
                    auto       last_time = steady_clock::now();

                    while (!m_impl->stop_flag)
                    {
                        auto   now = steady_clock::now();
                        double time_delta = duration<double>(now - last_time).count();
                        last_time = now;

                        instance.type->tick_fn(instance.ptr, time_delta);
                        next_tick_time += tick_interval;
                        hybrid_sleep_until(time_point_cast<steady_clock::duration>(next_tick_time));
                    }
                });
        }
    }

    void Engine::stop()
    {
        m_impl->stop_flag = true;

        for (auto& instance : m_impl->instances)
        {
            if (instance.type->stop_fn) instance.type->stop_fn(instance.ptr);
        }

        for (auto& t : m_impl->threads)
        {
            if (t.joinable()) t.join();
        }

        m_impl->threads.clear();
    }
}  // namespace robotick