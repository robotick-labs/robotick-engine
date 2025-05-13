#pragma once
#include "WorkloadGroup.hpp"

namespace robotick
{
    class ROBOTICK_API Engine
    {
    public:
        void start(WorkloadGroup &group, int rate_hz = 1000);
    };
}
