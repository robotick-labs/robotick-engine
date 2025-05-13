// ===============================
// Engine.cpp
// ===============================
#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"
#include "robotick/framework/ThreadUtils.h"
#include <chrono>
#include <windows.h>
#include <processthreadsapi.h>
#include <thread>
#include <atomic>
#include <vector>

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
        const auto &workloads = model.get_workloads();
        m_impl->threads.clear();
        m_impl->model = &model;

        for (auto &w : workloads)
        {
            m_impl->threads.emplace_back([w]()
                                         {
                w->pre_load();
                w->load(); });
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

        for (auto &w : m_impl->model->get_workloads())
        {
            w->setup();
        }
    }

    void Engine::start()
    {
        if (!m_impl->model)
            throw std::runtime_error("Engine::start called before load()");

        const auto &workloads = m_impl->model->get_workloads();
        m_impl->stop_flag = false;

        for (auto &w : workloads)
        {
            m_impl->threads.emplace_back([w, this]()
                                         {
                set_thread_name("robotick_" + w->get_name());

                double hz = w->get_tick_rate_hz();
                if (hz <= 0) return;

                using namespace std::chrono;
                auto tick_interval = duration<double>(1.0 / hz);
                InputBlock in;
                OutputBlock out;

                while (!m_impl->stop_flag) {
                    auto start_time = steady_clock::now();

                    w->pre_tick();
                    w->tick(in, out);
                    w->post_tick();

                    std::this_thread::sleep_until(start_time + tick_interval);
                } });
        }
    }

    void Engine::stop()
    {
        if (!m_impl->model)
            throw std::runtime_error("Engine::stop called before load()");

        m_impl->stop_flag = true;

        for (auto &workload : m_impl->model->get_workloads())
        {
            workload->stop();
        }

        for (auto &t : m_impl->threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }

        m_impl->threads.clear();
    }

}