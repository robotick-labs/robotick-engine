#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/utils_thread.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>

namespace robotick
{

    struct Engine::Impl
    {
        std::vector<std::thread> threads;
        std::atomic<bool> stop_flag = false;
        const Model *model = nullptr;
    };

    Engine::Engine() : m_impl(std::make_unique<Impl>()) {}

    Engine::~Engine()
    {
        m_impl->stop_flag = true;
        for (auto &t : m_impl->threads)
        {
            if (t.joinable())
                t.join();
        }
    }

    void Engine::load(const Model &model)
    {
        m_impl->threads.clear();
        m_impl->model = &model;

        const auto &handles = model.get_workloads();
        const auto &instances = model.factory().get_all();

        // Preload + load in parallel
        for (auto &h : handles)
        {
            const auto &instance = instances[h.index];
            m_impl->threads.emplace_back([&]()
                                         {
            if (instance.type->pre_load) instance.type->pre_load(instance.ptr);
            if (instance.type->load)     instance.type->load(instance.ptr); });
        }

        for (auto &t : m_impl->threads)
        {
            if (t.joinable())
                t.join();
        }

        m_impl->threads.clear();
    }

    void Engine::setup()
    {
        if (!m_impl->model)
            throw std::runtime_error("Engine::setup called before load()");

        const auto &factory = m_impl->model->factory();
        for (auto &h : m_impl->model->get_workloads())
        {
            const auto &instance = factory.get_all()[h.index];
            if (instance.type->setup)
                instance.type->setup(instance.ptr);
        }
    }

    void Engine::start()
    {
        if (!m_impl->model)
            throw std::runtime_error("Engine::start called before load()");

        const auto &handles = m_impl->model->get_workloads();
        const auto &instances = m_impl->model->factory().get_all();

        m_impl->stop_flag = false;

        for (auto &h : handles)
        {
            const auto &instance = instances[h.index];

            m_impl->threads.emplace_back([this, &instance]()
                                         {
            set_thread_name("robotick_" + std::string(instance.type->name));

            double hz = instance.tick_rate_hz;
            if (hz <= 0 || !instance.type->tick) return;

            using namespace std::chrono;
            const auto tick_interval = duration<double>(1.0 / hz);
            auto last_time = steady_clock::now();

            while (!m_impl->stop_flag) {
                auto start_time = steady_clock::now();
                double time_delta = duration<double>(start_time - last_time).count();
                last_time = start_time;

                instance.type->tick(instance.ptr, time_delta);
                std::this_thread::sleep_until(start_time + tick_interval);
            } });
        }
    }

    void Engine::stop()
    {
        if (!m_impl->model)
            throw std::runtime_error("Engine::stop called before load()");

        m_impl->stop_flag = true;

        const auto &factory = m_impl->model->factory();
        for (auto &h : m_impl->model->get_workloads())
        {
            const auto &instance = factory.get_all()[h.index];
            if (instance.type->stop)
                instance.type->stop(instance.ptr);
        }

        for (auto &t : m_impl->threads)
        {
            if (t.joinable())
                t.join();
        }

        m_impl->threads.clear();
    }

} // namespace robotick
