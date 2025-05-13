#include "robotick/framework/Engine.h"
#include "robotick/framework/Model.h"

#include <chrono>
#include <windows.h>
#include <processthreadsapi.h>  // Needed for SetThreadDescription

using namespace robotick;

Engine::Engine() = default;

Engine::~Engine() {
    m_stop_flag = true;
    for (auto& t : m_threads) {
        if (t.joinable())
            t.join();
    }
}

void Engine::load(const Model& model) {
    // Multithreaded load() for each workload
    const auto& workloads = model.get_workloads();
    m_threads.clear();

    for (auto& w : workloads) {
        m_threads.emplace_back([w]() {
            w->pre_load();
            w->load();
        });
    }

    for (auto& t : m_threads) {
        if (t.joinable())
            t.join();
    }
    m_threads.clear();
}

void Engine::setup(const Model& model) {
    for (auto& w : model.get_workloads()) {
        w->setup();
    }
}

void set_thread_name(const std::string& name) {
    auto hThread = GetCurrentThread();
    std::wstring wname(name.begin(), name.end());
    SetThreadDescription(hThread, wname.c_str());
}

void Engine::start(const Model& model) {
    m_model = &model;

    const auto& workloads = model.get_workloads();
    m_stop_flag = false;

    for (auto& w : workloads) {
        m_threads.emplace_back([w, this]() {
            
            set_thread_name("robotick_" + w->get_name());

            double hz = w->get_tick_rate_hz();
            if (hz <= 0) return;

            using namespace std::chrono;
            auto tick_interval = duration<double>(1.0 / hz);
            InputBlock in;
            OutputBlock out;

            while (!m_stop_flag) {
                auto start_time = steady_clock::now();

                w->pre_tick();
                w->tick(in, out);
                w->post_tick();

                std::this_thread::sleep_until(start_time + tick_interval);
            }
        });
    }
}

void Engine::stop() {
    m_stop_flag = true;

    for (auto& workload : m_model->get_workloads()) {
        workload->stop();  // stop any internal threads like in SyncedPair
    }

    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    m_threads.clear();
}    
