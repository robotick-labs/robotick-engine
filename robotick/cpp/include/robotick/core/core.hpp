#pragma once
#include "WorkloadGroup.hpp"

namespace robotick {
    ROBOTICK_API void run_ticker(WorkloadGroup& group, int rate_hz = 1000);
}
