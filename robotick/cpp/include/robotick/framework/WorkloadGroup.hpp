#pragma once
#include <vector>
#include <memory>
#include "IWorkload.hpp"

namespace robotick {

class ROBOTICK_API WorkloadGroup {
public:
    void add(std::shared_ptr<IWorkload> w) {
        workloads.push_back(std::move(w));
    }

    void setup_all() {
        for (auto& w : workloads) {
            w->pre_load();
            w->load();
            w->setup();
        }
    }

    void tick_all(const InputBlock& input) {
        for (auto& w : workloads) {
            w->pre_tick();
            w->tick(input, output);
            w->post_tick();
        }
    }

    const OutputBlock& get_output() const {
        return output;
    }

private:
    std::vector<std::shared_ptr<IWorkload>> workloads;
    OutputBlock output; // shared for now; later per-workload
};

}
