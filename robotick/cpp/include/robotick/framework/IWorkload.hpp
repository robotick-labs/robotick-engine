#pragma once

#include "robotick/framework/api.hpp"

namespace robotick
{
    struct InputBlock
    {
        double x = 0.0;
        double y = 0.0;
    };

    struct OutputBlock
    {
        double result = 0.0;
        double state = 0.0;
    };

    class ROBOTICK_API IWorkload
    {
    public:
        virtual ~IWorkload() = default;
        virtual void pre_load() {}
        virtual void load() {}
        virtual void setup() {}
        virtual void pre_tick() {}
        virtual void tick(const InputBlock &in, OutputBlock &out) = 0;
        virtual void post_tick() {}
    };
}
