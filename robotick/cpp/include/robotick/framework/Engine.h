#pragma once

#include "robotick/framework/api.h"

#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace robotick
{
    class Model;

    class ROBOTICK_API Engine
    {
    public:
        Engine();
        ~Engine();

        void load(const Model &model);  // multi-threaded workload load()
        void setup(const Model &model); // single-threaded workload setup()
        void start(const Model &model); // main tick loop
        void stop();

    private:
        const Model *m_model = nullptr;

        std::vector<std::thread> m_threads;
        std::atomic<bool> m_stop_flag = false;
    };

}
