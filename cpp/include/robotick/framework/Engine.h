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

        void load(const Model &model); // multi-threaded workload load()
        void setup();                  // single-threaded workload setup()
        void start();                  // main tick loop
        void stop();                   // single-thread stop() - cleanly terminates all threads

    private:
        ROBOTICK_DECLARE_PIMPL();
    };

}
