#include "robotick/core/core.hpp"
#include "robotick/core/WorkloadGroup.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace robotick
{
    void run_ticker(WorkloadGroup& group, int rate_hz) {
        InputBlock input{ 1.0, 2.0 }; // dummy values
        group.setup_all();
    
        const auto period = std::chrono::microseconds(1'000'000 / rate_hz);
    
        while (true) {
            auto start = std::chrono::steady_clock::now();
            group.tick_all(input);
    
            auto& out = group.get_output();
            std::cout << "Output: " << out.result << ", " << out.state << std::endl;
    
            std::this_thread::sleep_until(start + period);
        }
    }
}
