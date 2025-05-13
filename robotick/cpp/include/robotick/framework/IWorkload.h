#pragma once

#include "robotick/framework/api.h"

#include <unordered_map>
#include <string>

namespace robotick
{    
    struct InputBlock
    {
        std::unordered_map<std::string, double> writable;
    };
    
    struct OutputBlock
    {
        std::unordered_map<std::string, double> readable;
    };

    class ROBOTICK_API IWorkload
    {
    public:
        virtual ~IWorkload() = default;

        virtual double get_tick_rate_hz() = 0;
        virtual std::string get_name() = 0;
        
        virtual void pre_load() {}
        virtual void load() {}
        virtual void setup() {}
        virtual void pre_tick() {}
        virtual void tick(const InputBlock &in, OutputBlock &out, double time_delta) = 0;
        virtual void post_tick() {}
        virtual void stop() {}  // optional override for workloads with threads
    };
}
